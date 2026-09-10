#include <KIO/ThumbnailCreator>
#include <KColorScheme>
#include <KPluginFactory>

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace {

struct AudioData {
    std::vector<float> peaks;
    QImage cover;
};

struct Settings {
    int padding = 16;
    int waveformHeight = 72;
    int waveformLeftInset = 32;
    int waveformRightInset = 16;
    int cornerRadius = 16;
    int separatorHeight = 8;
    int minimumPreviewWidth = 129;
    bool artBackgroundSquareOnly = true;
    int bins = 1024;
    int samplesPerPeak = 2048;
    int minBarHeight = 2;
    int barGap = 0;
    double exponent = 1.0;
    QString imageMode = QStringLiteral("fit");
    QColor background = QColor(0, 0, 0, 0);
    QColor artBackground = QColor(QStringLiteral("#252a33"));
    QColor waveformColor = QColor(QStringLiteral("#5eead4"));
    QColor separatorColor = QColor(QStringLiteral("#252a33"));
    QColor accentColor = QColor(QStringLiteral("#5eead4"));
};

QColor systemAccentColor()
{
    return KColorScheme(QPalette::Active, KColorScheme::Selection).background(KColorScheme::NormalBackground).color();
}

QColor schemeColor(const QString &value, const QColor &accent)
{
    const QString token = value.trimmed().toLower();
    if (token == QStringLiteral("transparent")) {
        return QColor(0, 0, 0, 0);
    }
    if (token == QStringLiteral("accent") || token == QStringLiteral("systemaccent")) {
        return accent;
    }

    const QStringList parts = token.split(QLatin1Char('.'));
    if (parts.size() != 2) {
        const QColor explicitColor(value);
        return explicitColor.isValid() ? explicitColor : QColor{};
    }

    const QHash<QString, KColorScheme::ColorSet> sets{
        {QStringLiteral("view"), KColorScheme::View},
        {QStringLiteral("window"), KColorScheme::Window},
        {QStringLiteral("button"), KColorScheme::Button},
        {QStringLiteral("selection"), KColorScheme::Selection},
        {QStringLiteral("tooltip"), KColorScheme::Tooltip},
        {QStringLiteral("complementary"), KColorScheme::Complementary},
        {QStringLiteral("header"), KColorScheme::Header},
    };
    if (!sets.contains(parts.at(0))) {
        return {};
    }

    const KColorScheme scheme(QPalette::Active, sets.value(parts.at(0)));
    const QString role = parts.at(1);
    const QHash<QString, KColorScheme::BackgroundRole> backgrounds{
        {QStringLiteral("normalbackground"), KColorScheme::NormalBackground},
        {QStringLiteral("alternatebackground"), KColorScheme::AlternateBackground},
        {QStringLiteral("activebackground"), KColorScheme::ActiveBackground},
        {QStringLiteral("linkbackground"), KColorScheme::LinkBackground},
        {QStringLiteral("visitedbackground"), KColorScheme::VisitedBackground},
        {QStringLiteral("negativebackground"), KColorScheme::NegativeBackground},
        {QStringLiteral("neutralbackground"), KColorScheme::NeutralBackground},
        {QStringLiteral("positivebackground"), KColorScheme::PositiveBackground},
    };
    const QHash<QString, KColorScheme::ForegroundRole> foregrounds{
        {QStringLiteral("normaltext"), KColorScheme::NormalText},
        {QStringLiteral("inactivetext"), KColorScheme::InactiveText},
        {QStringLiteral("activetext"), KColorScheme::ActiveText},
        {QStringLiteral("linktext"), KColorScheme::LinkText},
        {QStringLiteral("visitedtext"), KColorScheme::VisitedText},
        {QStringLiteral("negativetext"), KColorScheme::NegativeText},
        {QStringLiteral("neutraltext"), KColorScheme::NeutralText},
        {QStringLiteral("positivetext"), KColorScheme::PositiveText},
    };
    if (backgrounds.contains(role)) {
        return scheme.background(backgrounds.value(role)).color();
    }
    if (foregrounds.contains(role)) {
        return scheme.foreground(foregrounds.value(role)).color();
    }
    if (role == QStringLiteral("focuscolor")) {
        return scheme.decoration(KColorScheme::FocusColor).color();
    }
    if (role == QStringLiteral("hovercolor")) {
        return scheme.decoration(KColorScheme::HoverColor).color();
    }
    const QHash<QString, KColorScheme::ShadeRole> shades{
        {QStringLiteral("lightshade"), KColorScheme::LightShade},
        {QStringLiteral("midlightshade"), KColorScheme::MidlightShade},
        {QStringLiteral("midshade"), KColorScheme::MidShade},
        {QStringLiteral("darkshade"), KColorScheme::DarkShade},
        {QStringLiteral("shadowshade"), KColorScheme::ShadowShade},
    };
    return shades.contains(role) ? scheme.shade(shades.value(role)) : QColor{};
}

Settings loadSettings()
{
    Settings settings;
    const QColor systemAccent = systemAccentColor();
    const QString homeConfigPath = QDir::homePath() + QStringLiteral("/.config/dolphin-waveform.conf");
    const QString standardConfigPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/dolphin-waveform.conf");
    const QString configPath = QFileInfo::exists(homeConfigPath) ? homeConfigPath : standardConfigPath;
    QSettings config(configPath, QSettings::IniFormat);
    config.beginGroup(QStringLiteral("Layout"));
    settings.padding = std::clamp(config.value(QStringLiteral("padding"), settings.padding).toInt(), 0, 256);
    settings.waveformHeight = std::clamp(config.value(QStringLiteral("waveformHeight"), settings.waveformHeight).toInt(), 16, 512);
    settings.waveformLeftInset = std::clamp(config.value(QStringLiteral("waveformLeftInset"), settings.waveformLeftInset).toInt(), 0, 512);
    settings.waveformRightInset = std::clamp(config.value(QStringLiteral("waveformRightInset"), settings.waveformRightInset).toInt(), 0, 512);
    settings.cornerRadius = std::clamp(config.value(QStringLiteral("cornerRadius"), settings.cornerRadius).toInt(), 0, 64);
    settings.separatorHeight = std::clamp(config.value(QStringLiteral("separatorHeight"), settings.separatorHeight).toInt(), 0, 128);
    settings.minimumPreviewWidth = std::clamp(config.value(QStringLiteral("minimumPreviewWidth"), settings.minimumPreviewWidth).toInt(), 0, 4096);
    settings.artBackgroundSquareOnly = config.value(QStringLiteral("artBackgroundSquareOnly"), settings.artBackgroundSquareOnly).toBool();
    settings.imageMode = config.value(QStringLiteral("imageMode"), settings.imageMode).toString().toLower();
    config.endGroup();
    config.beginGroup(QStringLiteral("Colors"));
    settings.accentColor = schemeColor(config.value(QStringLiteral("accent"), systemAccent.name()).toString(), systemAccent);
    settings.background = schemeColor(config.value(QStringLiteral("background"), settings.background.name()).toString(), settings.accentColor);
    settings.artBackground = schemeColor(config.value(QStringLiteral("artBackground"), settings.artBackground.name()).toString(), settings.accentColor);
    settings.waveformColor = schemeColor(config.value(QStringLiteral("waveform"), QStringLiteral("accent")).toString(), settings.accentColor);
    settings.separatorColor = schemeColor(config.value(QStringLiteral("separator"), settings.separatorColor.name()).toString(), settings.accentColor);
    config.endGroup();
    config.beginGroup(QStringLiteral("Waveform"));
    settings.bins = std::clamp(config.value(QStringLiteral("bins"), settings.bins).toInt(), 32, 4096);
    settings.samplesPerPeak = std::clamp(config.value(QStringLiteral("samplesPerPeak"), settings.samplesPerPeak).toInt(), 64, 65536);
    settings.minBarHeight = std::clamp(config.value(QStringLiteral("minBarHeight"), settings.minBarHeight).toInt(), 0, 32);
    settings.barGap = std::clamp(config.value(QStringLiteral("barGap"), settings.barGap).toInt(), 0, 16);
    settings.exponent = std::clamp(config.value(QStringLiteral("exponent"), settings.exponent).toDouble(), 0.1, 4.0);
    config.endGroup();
    if (!settings.background.isValid()) settings.background = QColor(QStringLiteral("#15171b"));
    if (!settings.artBackground.isValid()) settings.artBackground = QColor(QStringLiteral("#252a33"));
    if (!settings.waveformColor.isValid()) settings.waveformColor = settings.accentColor;
    if (!settings.separatorColor.isValid()) settings.separatorColor = QColor(QStringLiteral("#252a33"));
    if (!settings.accentColor.isValid()) settings.accentColor = QColor(QStringLiteral("#5eead4"));
    return settings;
}

QImage attachedPicture(AVFormatContext *format)
{
    for (unsigned int index = 0; index < format->nb_streams; ++index) {
        AVStream *stream = format->streams[index];
        if (!(stream->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            continue;
        }

        const AVPacket &packet = stream->attached_pic;
        QImage image = QImage::fromData(packet.data, packet.size);
        if (!image.isNull()) {
            return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
    }
    return {};
}

bool decodeAudio(const QString &path, AudioData &result, const Settings &settings)
{
    AVFormatContext *format = nullptr;
    if (avformat_open_input(&format, path.toUtf8().constData(), nullptr, nullptr) < 0) {
        return false;
    }

    const auto closeFormat = qScopeGuard([&] { avformat_close_input(&format); });
    if (avformat_find_stream_info(format, nullptr) < 0) {
        return false;
    }

    const int streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        return false;
    }

    AVStream *stream = format->streams[streamIndex];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        return false;
    }

    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    if (!codecContext || avcodec_parameters_to_context(codecContext, stream->codecpar) < 0) {
        avcodec_free_context(&codecContext);
        return false;
    }
    const auto closeCodec = qScopeGuard([&] { avcodec_free_context(&codecContext); });

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        return false;
    }

    AVChannelLayout outputLayout;
    av_channel_layout_default(&outputLayout, 1);
    SwrContext *resampler = nullptr;
    if (swr_alloc_set_opts2(&resampler,
                            &outputLayout,
                            AV_SAMPLE_FMT_FLT,
                            codecContext->sample_rate,
                            &codecContext->ch_layout,
                            codecContext->sample_fmt,
                            codecContext->sample_rate,
                            0,
                            nullptr) < 0
        || swr_init(resampler) < 0) {
        swr_free(&resampler);
        return false;
    }
    const auto closeResampler = qScopeGuard([&] { swr_free(&resampler); });

    result.cover = attachedPicture(format);
    std::vector<float> decodedPeaks;
    const int samplesPerPeak = settings.samplesPerPeak;
    float currentPeak = 0.0f;
    int samplesInPeak = 0;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    if (!packet || !frame) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        return false;
    }
    const auto freeFrames = qScopeGuard([&] {
        av_packet_free(&packet);
        av_frame_free(&frame);
    });

    auto consumeFrame = [&]() {
        const int outputSamples = swr_get_out_samples(resampler, frame->nb_samples);
        std::vector<float> samples(static_cast<size_t>(outputSamples));
        uint8_t *output[] = {reinterpret_cast<uint8_t *>(samples.data())};
        const int converted = swr_convert(resampler, output, outputSamples,
                                          const_cast<const uint8_t **>(frame->extended_data), frame->nb_samples);
        for (int sample = 0; sample < converted; ++sample) {
            currentPeak = std::max(currentPeak, std::abs(samples[static_cast<size_t>(sample)]));
            if (++samplesInPeak == samplesPerPeak) {
                decodedPeaks.push_back(currentPeak);
                currentPeak = 0.0f;
                samplesInPeak = 0;
            }
        }
    };

    while (av_read_frame(format, packet) >= 0) {
        if (packet->stream_index == streamIndex && avcodec_send_packet(codecContext, packet) >= 0) {
            while (avcodec_receive_frame(codecContext, frame) >= 0) {
                consumeFrame();
            }
        }
        av_packet_unref(packet);
    }
    avcodec_send_packet(codecContext, nullptr);
    while (avcodec_receive_frame(codecContext, frame) >= 0) {
        consumeFrame();
    }

    if (samplesInPeak > 0) {
        decodedPeaks.push_back(currentPeak);
    }
    if (decodedPeaks.empty()) {
        return false;
    }

    result.peaks.assign(static_cast<size_t>(settings.bins), 0.0f);
    for (size_t bucket = 0; bucket < result.peaks.size(); ++bucket) {
        const size_t first = bucket * decodedPeaks.size() / result.peaks.size();
        const size_t last = std::max(first + 1, (bucket + 1) * decodedPeaks.size() / result.peaks.size());
        for (size_t index = first; index < std::min(last, decodedPeaks.size()); ++index) {
            result.peaks[bucket] = std::max(result.peaks[bucket], decodedPeaks[index]);
        }
    }

    const float maximum = *std::max_element(result.peaks.begin(), result.peaks.end());
    if (maximum > 0.0f) {
        for (float &peak : result.peaks) {
            peak = std::pow(peak / maximum, settings.exponent);
        }
    }
    return true;
}

QImage renderThumbnail(const AudioData &data, const QSize requestedSize, qreal dpr, const Settings &settings)
{
    const int width = std::max(1, requestedSize.width());
    const int height = std::max(1, requestedSize.height());
    QImage image(QSize(width, height), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(settings.background);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const int margin = settings.padding;
    const int waveformHeight = std::min(settings.waveformHeight, std::max(16, height - margin * 2 - settings.separatorHeight));
    const QRect artRect(margin, margin, std::max(1, width - margin * 2), std::max(1, height - waveformHeight - margin * 2 - settings.separatorHeight));
    QRect artFrame = artRect;
    if (settings.artBackgroundSquareOnly) {
        const int side = std::min(artRect.width(), artRect.height());
        artFrame = QRect(artRect.center().x() - side / 2, artRect.center().y() - side / 2, side, side);
    }
    const QRect waveRect(std::min(settings.waveformLeftInset, width - 1), artRect.bottom() + settings.separatorHeight,
                         std::max(1, width - settings.waveformLeftInset - settings.waveformRightInset), waveformHeight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(settings.artBackground);
    painter.drawRoundedRect(artFrame, settings.cornerRadius, settings.cornerRadius);

    if (!data.cover.isNull()) {
        painter.save();
        const bool coverMode = settings.imageMode == QStringLiteral("cover");
        const QImage cover = data.cover.scaled(artFrame.size(), coverMode ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QRect fittedRect(artFrame.left() + (artFrame.width() - cover.width()) / 2,
                               artFrame.top() + (artFrame.height() - cover.height()) / 2,
                               cover.width(),
                               cover.height());
        QPainterPath clip;
        clip.addRoundedRect(coverMode ? artFrame : fittedRect, settings.cornerRadius, settings.cornerRadius);
        painter.setClipPath(clip);
        if (coverMode) {
            const QRect sourceRect((cover.width() - artFrame.width()) / 2,
                                   (cover.height() - artFrame.height()) / 2,
                                   artFrame.width(),
                                   artFrame.height());
            painter.drawImage(artFrame, cover, sourceRect);
        } else {
            painter.drawImage(fittedRect, cover);
        }
        painter.restore();
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(settings.waveformColor);
    painter.setRenderHint(QPainter::Antialiasing, settings.barGap > 0);
    const int center = waveRect.center().y();
    const qreal step = static_cast<qreal>(waveRect.width()) / data.peaks.size();
    for (size_t index = 0; index < data.peaks.size(); ++index) {
        const qreal x = waveRect.left() + index * step;
        const qreal barHeight = std::max<qreal>(2.0, data.peaks[index] * (waveRect.height() - 4));
        const qreal nextX = waveRect.left() + (index + 1) * step;
        const qreal barWidth = settings.barGap == 0 ? nextX - x : std::max<qreal>(1.0, step - settings.barGap);
        const qreal actualHeight = std::max<qreal>(settings.minBarHeight, barHeight);
        if (settings.barGap == 0) {
            painter.drawRect(QRectF(x, center - actualHeight / 2, barWidth, actualHeight));
        } else {
            painter.drawRoundedRect(QRectF(x, center - actualHeight / 2, barWidth, actualHeight), 1.0, 1.0);
        }
    }
    if (settings.cornerRadius > 0) {
        painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(0, 0, width, height), settings.cornerRadius, settings.cornerRadius);
    }
    return image;
}

}

class AudioThumbnailer final : public KIO::ThumbnailCreator
{
public:
    explicit AudioThumbnailer(QObject *parent, const QVariantList &args)
        : KIO::ThumbnailCreator(parent, args)
    {
    }

    KIO::ThumbnailResult create(const KIO::ThumbnailRequest &request) override
    {
        if (!request.url().isLocalFile() || !QFileInfo::exists(request.url().toLocalFile())) {
            return KIO::ThumbnailResult::fail();
        }

        const Settings settings = loadSettings();
        if (request.targetSize().width() < settings.minimumPreviewWidth) {
            return KIO::ThumbnailResult::fail();
        }
        AudioData data;
        if (!decodeAudio(request.url().toLocalFile(), data, settings)) {
            return KIO::ThumbnailResult::fail();
        }
        return KIO::ThumbnailResult::pass(renderThumbnail(data, request.targetSize(), request.devicePixelRatio(), settings));
    }
};

K_PLUGIN_CLASS_WITH_JSON(AudioThumbnailer, "audio_thumbnailer.json")

#include "audiothumbnailer.moc"
