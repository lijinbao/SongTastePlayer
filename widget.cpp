#include "widget.h"
#include "ui_widget.h"
#include <QDebug>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include "config.h"
#include "download.h"
#include "tool.h"

Widget::Widget(QWidget *parent) :QWidget(parent),ui(new Ui::Widget){

    ui->setupUi(this);
    this->songteste =new STPage();

    this->setWindowOpacity(1);
    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setWindowTitle(Config::title);
    this->setFixedSize(420,360);
    this->setFocus();
    this->setMouseTracking(true);
    ui->labelBg->setMouseTracking(true);
    ui->musicSlider->setMouseTracking(true);
    ui->sliderVolume->setMouseTracking(true);

    ui->labelVersion->setText(Config::title+" "+Config::version);

    //titleShow
    titleShow();

    //get config
    this->settings =new QSettings (QDir::homePath()+"/"+Config::config, QSettings ::IniFormat);
    if(!settings->contains("Player/downloadDir")){
        settings->setValue("Player/downloadDir",QDir::homePath());
        settings->setValue("Player/musicOrder",0);
        settings->setValue("Player/musicType",0);
        settings->setValue("Player/volume",50);
    }
    downloadDir=settings->value("Player/downloadDir").toString();
    //musicOrder=settings->value("Player/musicOrder").toInt();
    musicOrder=0;//暂时去掉播放模式的记忆
    ui->buttonPlayMode->setToolTip("顺序播放");

    //player
    palyNumber=0;
    musicListSize=0;
    buttonModel=false;//是否为点击了下一个或上一个，做为标记，会影响正常下的顺序播放。暂时的。
    scene=new QGraphicsScene();
    dragPosition=QPoint(-1, -1);//防止鼠标在控件上拖动窗口失效

    //table
    this->loadListView();
    connect(ui->tablemusiclist,&QTableWidget::cellDoubleClicked,[this](int row,int){
        emit this->signalPlayerMusic(row);
    });
    connect(this,&Widget::signalLoadList,&Widget::slotLoadList);

    //播放音乐
    connect(this,&Widget::signalPlayerMusic,&Widget::slotPlayMusic);
    connect(&player,&QMediaPlayer::stateChanged,this, &Widget::playerStateChanged);
    connect(&player, &QMediaPlayer::mediaStatusChanged, this, &Widget::playerMediaStatus);

    //进度条
    ui->musicSlider->setRange(0, 0);
    connect(ui->musicSlider,&QSlider::sliderMoved,[this](int position){
        player.setPosition(position);
    });
    connect(&player, &QMediaPlayer::positionChanged, [this](qint64 position){
        ui->musicSlider->setValue(position);
        ui->labelCurrentTime->setText(Tool::qint64ToTime(position).toString("mm:ss"));
    });
    connect(&player,  &QMediaPlayer::durationChanged, [this](qint64 duration){
        ui->musicSlider->setRange(0, duration);
        ui->labelTotalTime->setText(Tool::qint64ToTime(duration).toString("mm:ss"));
    });

    //按钮
    //ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(ui->buttonPlay,&QPushButton::clicked,this, &Widget::slotPlayButton);
    connect(ui->buttonPre, &QPushButton::clicked,this, &Widget::slotPreButton);
    connect(ui->buttonNext,&QPushButton::clicked,this, &Widget::slotNextButton);
    //download dir
    ui->pushButtonOpenDir->setText(downloadDir);
    connect(ui->pushButtonDownloadDir,&QPushButton::clicked,this, &Widget::slotSetDir);
    connect(ui->pushButtonOpenDir,&QPushButton::clicked,[this](){
        QDesktopServices::openUrl(this->downloadDir);
    });
    connect(ui->buttonPlayMode,&QPushButton::clicked,this,[this](){
        if(musicOrder==1){
            musicOrder=0;//顺序播放
            ui->buttonPlayMode->setStyleSheet("QPushButton{border-image: url(:/image/playmode_sequence.png);}");
            ui->buttonPlayMode->setToolTip("顺序播放");
        }else{
            musicOrder=1;//单曲循环
            ui->buttonPlayMode->setStyleSheet("QPushButton{border-image: url(:/image/playmode_repeatone.png);}");
            ui->buttonPlayMode->setToolTip("单曲循环");
        }
        //settings->setValue("Player/musicOrder",musicOrder);
    });

    //音量
    ui->sliderVolume->setRange(0,100);
    ui->sliderVolume->setValue(0);
    connect(ui->sliderVolume, &QSlider::valueChanged,[this](int value){
        player.setVolume(value);
        this->settings->setValue("Player/volume",value);
    });
    ui->sliderVolume->setValue(settings->value("Player/volume").toInt());
    //contentmenu
    this->contentMenu();
    this->showTrayIcon();

    //list type
    ui->comboMusicType->insertItems(0,songteste->typeLists());
    ui->comboMusicType->setCurrentIndex(-1);
    connect(ui->comboMusicType,static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),[this](int i){
        settings->setValue("Player/musicType",i);
        slotLoadList(i);
    });//不明白为什么QComboBox要类型转换，其它的都不用，static_cast<void (QComboBox::*)(int)>
    ui->comboMusicType->setCurrentIndex(settings->value("Player/musicType").toInt());

    //download
    downloadingRow=-1;//默认-1当大于-1即有文件下载
}

Widget::~Widget(){
    delete ui;
}

void Widget::loadListView(){
    ui->tablemusiclist->setSelectionBehavior(QAbstractItemView::SelectRows); //整行选中的方式
    ui->tablemusiclist->setEditTriggers(QAbstractItemView::NoEditTriggers);//禁止编辑
    ui->tablemusiclist->setSelectionMode(QAbstractItemView::SingleSelection); //设置为可以选中多个目标
    ui->tablemusiclist->verticalHeader()->setVisible(false); //隐藏行号
    ui->tablemusiclist->horizontalHeader()->setVisible(false); //隐藏行表头
    ui->tablemusiclist->setColumnWidth(0,340);
    ui->tablemusiclist->setColumnWidth(1,60);
    ui->tablemusiclist->setShowGrid(false);
    ui->tablemusiclist->setFocusPolicy(Qt::NoFocus);
    ui->tablemusiclist->setStyleSheet("selection-background-color:#9ED3FE");  //设置选中行颜色
    ui->tablemusiclist->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tablemusiclist,&QTableWidget::customContextMenuRequested, this, &Widget::tableContentMenu);

    ui->tableDownloadList->setSelectionBehavior(QAbstractItemView::SelectRows); //整行选中的方式
    ui->tableDownloadList->setEditTriggers(QAbstractItemView::NoEditTriggers);//禁止编辑
    ui->tableDownloadList->setSelectionMode(QAbstractItemView::SingleSelection); //设置为可以选中多个目标
    ui->tableDownloadList->verticalHeader()->setVisible(false); //隐藏行号
    ui->tableDownloadList->horizontalHeader()->setVisible(false); //隐藏行表头
    ui->tableDownloadList->setColumnWidth(0,300);
    ui->tableDownloadList->setColumnWidth(1,100);
    ui->tableDownloadList->setColumnWidth(2,60);
    ui->tableDownloadList->setColumnWidth(3,30);//下载状态 1,等待，2正在下载，3下载完成 ，下载出错
    ui->tableDownloadList->setShowGrid(false);
    ui->tableDownloadList->setFocusPolicy(Qt::NoFocus);
    ui->tableDownloadList->setStyleSheet("selection-background-color:#9ED3FE");  //设置选中行颜色
    ui->tableDownloadList->setColumnHidden(2,true);
    ui->tableDownloadList->setColumnHidden(3,true);
    ui->tableDownloadList->setContextMenuPolicy(Qt::CustomContextMenu);
}

void Widget::slotLoadList(int type){
    ui->labelMessage->setText(ui->comboMusicType->currentText()+"列表加载中...");

    this->musicLists=this->songteste->musicLists(type);
    this->musicListSize=this->musicLists.size();
    if(musicListSize>0){
        ui->tablemusiclist->setRowCount(musicListSize);
        for(int i=0;i<musicListSize;i++){
            STModel song=this->musicLists.at(i);
            this->ui->tablemusiclist->setItem(i,0,new QTableWidgetItem(song.name));
            this->ui->tablemusiclist->setItem(i,1,new QTableWidgetItem(song.author));
            this->ui->tablemusiclist->setRowHeight(i,22);
        }
        ui->labelMessage->setText(ui->comboMusicType->currentText()+"列表加载完成");
    }else{
        ui->labelMessage->setText(ui->comboMusicType->currentText()+"列表加载失败");
    }
    this->palyNumber=0;//刷新列表后重新计数为-1，播放完后会加1，重新开始播放
}

void Widget::setRowColor(QTableWidget *table,int row, QColor textcolor,QColor backcolor){
    int size=2;//ui->tablemusiclist->columnCount();
    QTableWidgetItem *item;
    for (int col=0; col<size; col++){
        item =table->item(row, col);
        item->setBackgroundColor(backcolor);
        item->setTextColor(textcolor);
    }
}

void Widget::slotPlayMusic(int id){
    if(id<0||id>=this->musicListSize){
        return ;
    }

    STModel song=this->musicLists.at(id);
    QString songurl=this->songteste->songUrl(song.id);

    player.setMedia(QUrl(songurl));
    player.play();

    //
    buttonModel=false;

    //设置歌名
    ui->labelName->setText(song.name);
    ui->labelAuthor->setText(song.author);

    //播放时改变列表中的行颜色
    setRowColor(ui->tablemusiclist,palyNumber,QColor("#999"),QColor("#fff"));
    this->palyNumber=id;
    setRowColor(ui->tablemusiclist,id,QColor("#fff"),QColor("#0579C7"));

    //设置头像
    pixmap.loadFromData(songteste->userImage(song.image));
    ui->labelImage->setPixmap(pixmap);
}

void Widget::playerStateChanged(QMediaPlayer::State state){
    switch(state) {
    case QMediaPlayer::PlayingState:
        ui->buttonPlay->setStyleSheet("QPushButton{border-image: url(:/image/button_pause.png);}");
        //ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        break;
    case QMediaPlayer::PausedState:
        ui->buttonPlay->setStyleSheet("QPushButton{border-image: url(:/image/button_play.png);}");
        //ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        break;
    default:
        ui->buttonPlay->setStyleSheet("QPushButton{border-image: url(:/image/button_stop.png);}");
        break;
    }
}

void Widget::slotPlayButton(){
    switch(player.state()){
        case QMediaPlayer::PlayingState:
            player.pause();
            break;
        case QMediaPlayer::StoppedState:
            if(palyNumber==0&&this->musicListSize>0){
                emit this->signalPlayerMusic(0);
            }
        default:
            player.play();
            break;
    }
}

void Widget::slotPreButton(){    
    if(this->musicListSize>0){
        buttonModel=true;
        emit this->signalPlayerMusic(palyNumber-1);
    }
}

void Widget::slotNextButton(){    
    if(this->musicListSize>0){
        buttonModel=true;
        emit this->signalPlayerMusic(palyNumber+1);
    }
}

void Widget::playerMediaStatus(QMediaPlayer::MediaStatus stats){
    switch(stats){
        case QMediaPlayer::EndOfMedia:
            if(musicOrder==1){
                emit this->signalPlayerMusic(palyNumber);
            }else{
                emit this->signalPlayerMusic(palyNumber+1);
            }
            break;
        case QMediaPlayer::LoadingMedia:
            ui->labelMessage->setText("正在缓冲...");
            break;
        case QMediaPlayer::InvalidMedia:
            player.stop();
            ui->labelMessage->setText("无法连接当前音乐");
            break;
        case QMediaPlayer::BufferedMedia:
            ui->labelMessage->setText("正在播放...");
            break;
        default:

            break;
    }
    qDebug()<<stats;
}

void Widget::slotHideList(){
    if(ui->stackedWidget->isHidden()){
        this->setFixedHeight(360);
        ui->labelBg->setFixedHeight(360);
        ui->stackedWidget->show();
    }else{
        this->setFixedHeight(60);
        ui->labelBg->setFixedHeight(60);
        ui->stackedWidget->hide();
    }
}

void Widget::titleShow(){
    ui->labelName->show();
    ui->labelAuthor->show();
    ui->buttonNext->hide();
    ui->buttonPre->hide();
    ui->buttonPlay->hide();
    ui->sliderVolume->hide();
    ui->labelVolumeSmall->hide();
}

void Widget::titleHide(){
    ui->labelAuthor->hide();
    ui->labelName->hide();
    ui->labelAuthor->hide();
    ui->buttonNext->show();
    ui->buttonPre->show();
    ui->buttonPlay->show();
    ui->sliderVolume->show();
    ui->labelVolumeSmall->show();
}

void Widget::downloadMusic(int i){
    STModel song=musicLists.at(i);

    int tsize=ui->tableDownloadList->rowCount();

    ui->tableDownloadList->setRowCount(tsize+1);
    ui->tableDownloadList->setItem(tsize,0,new QTableWidgetItem(song.name));
    ui->tableDownloadList->setItem(tsize,1,new QTableWidgetItem("等待下载"));
    ui->tableDownloadList->setItem(tsize,2,new QTableWidgetItem(song.id));
    ui->tableDownloadList->setItem(tsize,3,new QTableWidgetItem("wait"));
    ui->tableDownloadList->setRowHeight(tsize,22);

    if(downloadingRow<0){
        this->downloadManager();
    }
}

void Widget::downloadManager(){
    int tsize=0;
    Download *download=new Download();
    connect(download,&Download::progress,this,&Widget::downloadProgress);
    while(1){
        if(tsize>=ui->tableDownloadList->rowCount()){
            break;
        }
        if(ui->tableDownloadList->item(tsize,3)->text()=="wait"){
            downloadingRow=tsize;

            QString url=songteste->songUrl(ui->tableDownloadList->item(tsize,2)->text());
            QString filename=(this->downloadDir)+"/"+ui->tableDownloadList->item(tsize,0)->text()+".mp3";
            qDebug()<<"downloading : "<<filename;
            download->run(url,filename);
            ui->tableDownloadList->setItem(tsize,1,new QTableWidgetItem("下载完成"));
            ui->tableDownloadList->setItem(tsize,3,new QTableWidgetItem("downloaded"));
            setRowColor(ui->tableDownloadList,tsize,QColor("#999"),QColor("#fff"));
        }
        tsize++;
    }
    download->deleteLater();
    downloadingRow=-1;
}

void Widget::downloadProgress(qint64 recieved, qint64 total){
    QString a=Tool::qint64ToStringKb(recieved)+"KB/"+Tool::qint64ToStringKb(total)+"KB";
    ui->tableDownloadList->setItem(downloadingRow,1,new QTableWidgetItem(a));
    setRowColor(ui->tableDownloadList,downloadingRow,QColor("#fff"),QColor("#0579C7"));
}

void Widget::slotSetDir(){
    QString dir = QFileDialog::getExistingDirectory(this,tr("打开目录"),this->downloadDir,QFileDialog::ShowDirsOnly);
    if(dir!=""){
        downloadDir=dir;
        settings->setValue("Player/downloadDir",dir);
        ui->pushButtonOpenDir->setText(dir);
    }
}

//system
void Widget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton){
        dragPosition = QPoint(-1, -1);
        event->accept();
    }
}

void Widget::mousePressEvent(QMouseEvent * event){
    if (event->button() == Qt::LeftButton){
         dragPosition    = event->globalPos() - frameGeometry().topLeft();
         //globalPos()获取根窗口的相对路径，frameGeometry().topLeft()获取主窗口左上角的位置
         event->accept();   //鼠标事件被系统接收
    }
}

void Widget::mouseMoveEvent(QMouseEvent * event){
    if(1<event->y()&&event->y()<40&&61<event->x()&&event->x()<415){
       titleHide();
    }else{
       titleShow();
    }

    if (event->buttons()==Qt::LeftButton && dragPosition != QPoint(-1, -1)){
        move(event->globalPos() - dragPosition);
        event->accept();
    }
}

//双击显示隐藏列表
void Widget::mouseDoubleClickEvent(QMouseEvent *event){
    slotHideList();
    event->accept();
}

void Widget::contentMenu(){
    trayMenu = new QMenu(this);//创建菜单
    QAction *Tray_quit = new QAction("退出", this);
    //Tray_quit->setIcon(QIcon(":/image/image/delete.png"));
    connect(Tray_quit,&QAction::triggered, [this](){
        this->close();
        QApplication::quit();
    });

    QAction *Tray_homepage = new QAction("检查更新", this);
    //Tray_flux_day->setIcon(QIcon(":/image/image/checkmark.png"));
    connect(Tray_homepage,&QAction::triggered, [this](){
        QDesktopServices::openUrl(QUrl(Config::homepage));
    });

    QAction *menuWindowsMinimized = new QAction("隐藏主界面", this);
    connect(menuWindowsMinimized,&QAction::triggered, [this](){
        this->hide();
    });

    QAction *menuHideList = new QAction("隐藏/显示列表", this);
    connect(menuHideList,&QAction::triggered, this,&Widget::slotHideList);

    QAction *menuMusiclist = new QAction("音乐列表", this);
    connect(menuMusiclist,&QAction::triggered,[this](){
        ui->stackedWidget->setCurrentIndex(0);
    });

    QAction *menuDownload = new QAction("下载列表", this);
    connect(menuDownload,&QAction::triggered, [this](){
        ui->stackedWidget->setCurrentIndex(1);
    });

    trayMenu->addAction(menuHideList);
    trayMenu->addAction(menuWindowsMinimized);
    trayMenu->addSeparator();
    trayMenu->addAction(menuMusiclist);
    trayMenu->addAction(menuDownload);
    trayMenu->addSeparator();
    trayMenu->addAction(Tray_homepage);
    trayMenu->addAction(Tray_quit);
}

void Widget::tableContentMenu(const QPoint &pos){
    QAction *menuRefreshList = new QAction("刷新列表", this);
    connect(menuRefreshList,&QAction::triggered, [this](){
        emit slotLoadList(ui->comboMusicType->currentIndex());
    });

    QAction *downMusic = new QAction("下载", this);
    QMenu menu(this);
    menu.addAction(downMusic);
    menu.addAction(menuRefreshList);
    QAction *m=menu.exec(ui->tablemusiclist ->viewport()->mapToGlobal(pos));
    if(m==downMusic){
        downloadMusic(ui->tablemusiclist->itemAt(pos)->row());
    }
}

void Widget::contextMenuEvent(QContextMenuEvent *){
    trayMenu->exec(this->cursor().pos()); //关联到光标
}

void Widget::showTrayIcon(){
    QIcon icon(":/image/logo.png");
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(icon);
    trayIcon->show();
    trayIcon->setContextMenu(trayMenu);//将创建菜单作为系统托盘菜单
    trayIcon->setToolTip(Config::title);
    trayIcon->showMessage(Config::title,"我在这~~", QSystemTrayIcon::Information, 5000);
    connect(trayIcon,&QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::Trigger&&this->isHidden()) {
            this->showNormal();
        }
    });
}

void Widget::keyPressEvent(QKeyEvent *k){
    if (k->modifiers() == Qt::ControlModifier){
        switch(k->key()){
            case Qt::Key_Left: //上一首
                slotPreButton();
                break;
            case Qt::Key_Right://下一首
                slotNextButton();
                break;
            case Qt::Key_Up://音量大
                ui->sliderVolume->setValue(ui->sliderVolume->value()+5);
                break;
            case Qt::Key_Down://音量小
                ui->sliderVolume->setValue(ui->sliderVolume->value()-5);
                break;
            default:
                break;
        }
    }else{
        switch(k->key()){
            case Qt::Key_Space:  //播放暂停
                slotPlayButton();
                break;
            default:
                break;
        }
    }
}

