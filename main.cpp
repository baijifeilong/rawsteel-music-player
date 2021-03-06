#include <QtCore/QFile>
#include <QtCore/QMimeData>
#include <QtGui/QDragEnterEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QDial>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QMediaPlaylist>
#include <QtCore/QTime>

struct LyricEntry {
    int start = 0;
    QString text = "";
};

QList<LyricEntry> parseLyric(const QString &lyric) {
    QList<LyricEntry> lyrics;
    auto lines = lyric.split('\n');
    QRegExp regExp("\\\[(\\\d{2}):([\\\d\\\.]{5})\\\](.+)");
    for (auto &line: lyric.split("\n")) {
        line = line.trimmed();
        if (line.size() > 10 && line.at(0) == '[' && line.at(10) == '[') {
            line = line.mid(10);
        }
        if (line.indexOf(regExp) >= 0) {
            auto minutes = regExp.cap(1);
            auto seconds = regExp.cap(2);
            auto text = regExp.cap(3);
            auto millis = int((minutes.toInt() * 60 + seconds.toFloat()) * 1000);

            LyricEntry entry;
            entry.start = millis;
            entry.text = text;
            lyrics.append(entry);
        }
    }
    return lyrics;
}

class MySlider : public QSlider {
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            if (orientation() == Qt::Vertical)
                setValue(minimum() + ((maximum() - minimum()) * (height() - event->y())) / height());
            else
                setValue(minimum() + ((maximum() - minimum()) * event->x()) / width());

            event->accept();
        }
        QSlider::mousePressEvent(event);
    }
};

QIcon getIcon(const QString &name) {
    return QIcon::fromTheme(name, QIcon(QString("icons/%1").arg(name)));
}

class MyMusicPlayer : QWidget {
public:
    MyMusicPlayer() { // NOLINT
        setupLayout();
        setupPlayer();
        setupEvents();
        setupInitialState();
    }

private:
    QToolButton *playButton;
    QToolButton *prevButton;
    QToolButton *nextButton;
    QToolButton *playbackModeButton;
    QMediaPlaylist *playlist;
    QMediaPlayer *mediaPlayer;
    QListWidget *listWidget;
    QLabel *lyricLabel;
    QScrollArea *lyricWrapper;
    MySlider *progressSlider;
    QLabel *progressLabel;
    QDial *dial;
    QFile *playlistFile;
    QList<LyricEntry> lyrics;
    QList<QUrl> urls;
    int lastLyricLine = -1;


    QIcon mediaPlaybackStartIcon;
    QIcon mediaPlaybackPauseIcon;
    QIcon mediaSkipBackwardIcon;
    QIcon mediaSkipForwardIcon;
    QIcon mediaPlaylistShuffleIcon;
    QIcon mediaPlaylistRepeatIcon;

    void setupLayout() {
        mediaPlaybackStartIcon = getIcon("media-playback-start");
        mediaPlaybackPauseIcon = getIcon("media-playback-pause");
        mediaSkipBackwardIcon = getIcon("media-skip-backward");
        mediaSkipForwardIcon = getIcon("media-skip-forward");
        mediaPlaylistShuffleIcon = getIcon("media-playlist-shuffle");
        mediaPlaylistRepeatIcon = getIcon("media-playlist-repeat");

        QSize iconSize(50, 50);

        playButton = new QToolButton;
        playButton->setIcon(mediaPlaybackStartIcon);
        playButton->setIconSize(iconSize);
        playButton->setAutoRaise(true);

        prevButton = new QToolButton;
        prevButton->setIcon(mediaSkipBackwardIcon);
        prevButton->setIconSize(iconSize);
        prevButton->setAutoRaise(true);

        nextButton = new QToolButton;
        nextButton->setIcon(mediaSkipForwardIcon);
        nextButton->setIconSize(iconSize);
        nextButton->setAutoRaise(true);

        playbackModeButton = new QToolButton;
        playbackModeButton->setIcon(mediaPlaylistShuffleIcon);
        playbackModeButton->setIconSize(iconSize);
        playbackModeButton->setAutoRaise(true);

        auto *topLayout = new QHBoxLayout(this);
        progressSlider = new MySlider;
        progressSlider->setOrientation(Qt::Horizontal);
        progressSlider->setMaximum(300);
        progressLabel = new QLabel("00:00/00:00");
        dial = new QDial;
        dial->setFixedHeight(50);

        topLayout->addWidget(playButton);
        topLayout->addWidget(prevButton);
        topLayout->addWidget(nextButton);
        topLayout->addWidget(progressSlider);
        topLayout->addWidget(progressLabel);
        topLayout->addWidget(playbackModeButton);
        topLayout->addWidget(dial);
        auto *topWidget = new QWidget();
        topWidget->setLayout(topLayout);

        listWidget = new QListWidget();
        listWidget->horizontalScrollBar()->setStyleSheet("QScrollBar {height: 0px}");
        lyricLabel = new QLabel("<center>Lyrics...</center>");
        auto font = lyricLabel->font();
        font.setPointSize(14);
        lyricLabel->setFont(font);
        lyricWrapper = new QScrollArea;
        lyricWrapper->setWidgetResizable(true);
        lyricWrapper->setWidget(lyricLabel);
        lyricWrapper->horizontalScrollBar()->setStyleSheet("QScrollBar {height: 0px}");
        lyricWrapper->verticalScrollBar()->setStyleSheet("QScrollBar {width: 0px}");
        auto *bodyLayout = new QHBoxLayout();
        bodyLayout->addWidget(listWidget, 1);;
        bodyLayout->addWidget(lyricWrapper, 1);;
        auto *bodyWidget = new QWidget();
        bodyWidget->setLayout(bodyLayout);

        auto *rootLayout = new QVBoxLayout();
        rootLayout->addWidget(bodyWidget, 1);
        rootLayout->addWidget(topWidget);

        this->resize(999, 666);
        this->setLayout(rootLayout);
        this->show();

        setAcceptDrops(true);
    }

    void setupPlayer() {
        playlist = new QMediaPlaylist();
        playlist->setPlaybackMode(QMediaPlaylist::PlaybackMode::Random);
        mediaPlayer = new QMediaPlayer();
        mediaPlayer->setPlaylist(playlist);
    }

    void setupEvents() {
        QObject::connect(playButton, &QToolButton::clicked, [this](bool) { togglePlay(); });
        QObject::connect(prevButton, &QToolButton::clicked, [this](bool) { playPrevious(); });
        QObject::connect(nextButton, &QToolButton::clicked, [this](bool) { playNext(); });
        QObject::connect(playbackModeButton, &QToolButton::clicked, [this](bool) { togglePlaybackMode(); });
        QObject::connect(listWidget, &QListWidget::itemDoubleClicked,
                         [this](QListWidgetItem *item) { playItem(item); });
        QObject::connect(playlist, &QMediaPlaylist::mediaInserted,
                         [this](int start, int end) { updateListWidget(start, end); });
        QObject::connect(dial, &QDial::valueChanged, mediaPlayer, &QMediaPlayer::setVolume);

        QObject::connect(mediaPlayer, &QMediaPlayer::stateChanged, [this](QMediaPlayer::State state) {
            if (state == QMediaPlayer::State::PlayingState) {
                this->playButton->setIcon(mediaPlaybackPauseIcon);
            } else {
                this->playButton->setIcon(mediaPlaybackStartIcon);
            }
        });

        QObject::connect(progressSlider, &QSlider::valueChanged, [this](int value) {
            this->mediaPlayer->blockSignals(true);
            this->mediaPlayer->setPosition(mediaPlayer->duration() * value / 300);
            this->mediaPlayer->blockSignals(false);
        });
        QObject::connect(mediaPlayer, &QMediaPlayer::positionChanged, [this](qint64 position) {
            if (mediaPlayer->duration() == 0) {
                return;
            }
            this->progressSlider->blockSignals(true);
            this->progressSlider->setValue(static_cast<int>(position * 300 / mediaPlayer->duration()));
            auto total = mediaPlayer->duration() / 1000;
            auto current = position / 1000;
            QString str = QString("%1:%2/%3:%4")
                    .arg(current / 60)
                    .arg(current % 60, 2, 10, QLatin1Char('0'))
                    .arg(total / 60)
                    .arg(total % 60, 2, 10, QLatin1Char('0'));

            // Weired bug: sprintf output wrong result in windows
            // str.sprintf("%d:%d/%d:%d", current / 60, current % 60, total / 60, total % 60);
            progressLabel->setText(str);
            this->progressSlider->blockSignals(false);
            this->refreshLyric();
        });

        QObject::connect(playlist, &QMediaPlaylist::currentIndexChanged, [this](int) {
            this->onPlayAnother();
        });
    }

    void onPlayAnother() {
        progressSlider->setValue(0);
        listWidget->setCurrentRow(playlist->currentIndex());
        updateLyric();
        auto media = playlist->currentMedia();
        auto mediaName = media.canonicalUrl().fileName().replace(QRegExp("\\\.[^\.]+$"), "");
        setWindowTitle(mediaName);
    }

    void setupInitialState() {
        dial->setValue(50);

        playlistFile = new QFile("playlist");
        playlistFile->open(QIODevice::Append);
        playlistFile->close();
        playlistFile->open(QIODevice::ReadOnly | QIODevice::Text);
        QTextStream stream(playlistFile);
        stream.setCodec("UTF-8");
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (!line.isEmpty()) {
                playlist->addMedia(QUrl::fromLocalFile(line));
            }
        }
        playlistFile->close();
    }

    void togglePlay() {
        if (mediaPlayer->state() == QMediaPlayer::State::PlayingState) {
            mediaPlayer->pause();
        } else {
            mediaPlayer->play();
        }
    }

    void togglePlaybackMode() {
        if (playlist->playbackMode() == QMediaPlaylist::PlaybackMode::Random) {
            playlist->setPlaybackMode(QMediaPlaylist::PlaybackMode::Loop);
            playbackModeButton->setIcon(mediaPlaylistRepeatIcon);
        } else {
            playlist->setPlaybackMode(QMediaPlaylist::PlaybackMode::Random);
            playbackModeButton->setIcon(mediaPlaylistShuffleIcon);
        }
    }

    void playNext() {
        playlist->next();
        mediaPlayer->play();
    }

    void playPrevious() {
        playlist->previous();
        mediaPlayer->play();
    }

    void playItem(QListWidgetItem *item) {
        playlist->setCurrentIndex(listWidget->row(item));
        mediaPlayer->play();
    }

    void updateListWidget(int start, int end) {
        for (int i = start; i <= end; ++i) {
            auto media = playlist->media(i);
            auto filename = media.canonicalUrl().fileName().replace(QRegExp("\\\.[^\.]+$"), "");
            auto parts = filename.replace(" ", "").split("-").filter(QRegExp(".+"));
            QString artist = "Unknown";
            QString title = "Unknown";
            if (parts.size() == 1) {
                title = parts.first();
            } else if (parts.size() > 1) {
                artist = parts.first();
                title = parts[1];
            }

            int artistLength = 0;
            for (auto ch : artist) {
                artistLength += ch.unicode() > 999 ? 2 : 1;
            }
            auto tabs = artistLength > 9 ? "\t" : "\t\t";
            auto str = QString("%1\t%2%3%4").arg(QString::number(i), artist, tabs, title);
            listWidget->addItem(str);
        }
    }

    void updateLyric() {
        auto currentMedia = playlist->currentMedia();
        auto url = currentMedia.canonicalUrl();
        auto path = QString(url.toLocalFile());
        path.replace(QRegExp("(wma|mp3|ogg)$"), "lrc");
        QFile file(path);
        lyrics.clear();
        if (!file.exists()) {
            lyricLabel->setText("Lyric not exist: " + path);
        } else if (!file.open(QFile::ReadOnly)) {
            lyricLabel->setText("Lyric open failed: " + path);
        } else {
            QTextStream in(&file);
            in.setCodec("GBK");
            lyrics.append(parseLyric(in.readAll()));
            refreshLyric(true);
        }
    }

    void refreshLyric(bool forceRefresh = false) {
        if (lyrics.isEmpty()) {
            return;
        }
        QString newLyric;
        int currLine = currentLyricLine();
        if (currLine == lastLyricLine && !forceRefresh) {
            return;
        }
        lastLyricLine = currLine;
        for (int i = 0; i < lyrics.size(); ++i) {
            auto lyric = lyrics.at(i);
            if (i == currLine) {
                newLyric.append(QString("<center><b>%1</b></center>").arg(lyric.text));
            } else {
                newLyric.append(QString("<center>%1</center>").arg(lyric.text));
            }
        }
        lyricLabel->setText(newLyric);
        lyricWrapper->verticalScrollBar()->setValue(
                lyricLabel->height() * currLine / lyrics.size() - lyricWrapper->height() / 2);
    }

    int currentLyricLine() {
        if (lyrics.isEmpty()) {
            return 0;
        }
        auto currentPosition = static_cast<int>(mediaPlayer->position());
        if (currentPosition < lyrics.first().start) {
            return 0;
        }
        for (int i = 0; i < lyrics.size() - 1; ++i) {
            if (currentPosition > lyrics.at(i).start && currentPosition < lyrics.at(i + 1).start) {
                return i;
            }
        }
        return lyrics.size() - 1;
    }

protected:

    /// Accept dragEnter
    /// When dragging thousands of files, the urls may be empty sometimes.
    /// So save them for reading in dropEvent
    void dragEnterEvent(QDragEnterEvent *event) override {
        event->accept();
        auto urls = event->mimeData()->urls();
        qDebug() << "Drag size" << event->mimeData()->urls().size();
        this->urls.clear();
        for (const auto &url: urls) {
            if (url.toString().contains(QRegExp("(mp3|wma|ogg)$"))) {
                this->urls.append(url);
            }
        }
    }

    /// Accept drop
    void dropEvent(QDropEvent *event) override {
        qDebug() << "Drop size" << event->mimeData()->urls().size();
        playlistFile->open(QIODevice::Append | QIODevice::Text);
        QTextStream stream(playlistFile);
        stream.setCodec("UTF-8");
        for (const auto &url: this->urls) {
            playlist->addMedia(QUrl(url));
            stream << url.path() << "\n";
        }
        playlistFile->close();
    }

private:
    // Resize for lyric view
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (!this->lyrics.isEmpty()) {
            this->refreshLyric(true);
        }
    }

};


int main(int argc, char **argv) {
    // Use real random playback for Qt version lower than 5.10
    srand(static_cast<unsigned int>(QTime::currentTime().msec()));
    QApplication application(argc, argv);
    application.setApplicationName("Rawsteel");
    application.setApplicationDisplayName("Rawsteel Music Player");
    application.setWindowIcon(getIcon("audio-headphones"));
    new MyMusicPlayer;
    return application.exec();
}
