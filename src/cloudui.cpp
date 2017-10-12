#include "cloudui.h"
#include "ui_cloudui.h"

#include <QList>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMessageBox>
#include <QModelIndexList>
#include <QModelIndex>
#include <QSettings>
#include <QUrlQuery>

bool isLetterNumUnder(QString str);

void popMsg(QString title, QString msg, bool isError);

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "globals.h"

CloudUI::CloudUI(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CloudUI)
{
    ui->setupUi(this);


    packetsToImport.clear();
    packetsToExport.clear();
    packetsFound.clear();
    packetSets.clear();

    ui->passwordConfirmEdit->hide();
    ui->passwordConfirmLabel->hide();

    ui->viewPublicButton->hide();


    ui->cloudTabWidget->setCurrentIndex(0);


    ui->createAccountButton->setStyleSheet(HYPERLINKSTYLE);
    ui->createAccountButton->setIcon( QIcon("://icons/ic_person_black_24dp_2x.png"));
    ui->createAccountButton->setFlat(true);
    ui->createAccountButton->setCursor(Qt::PointingHandCursor);



    http = new QNetworkAccessManager(this); //Cloud UI http object

    if(! connect(http, SIGNAL(finished(QNetworkReply*)),
                     this, SLOT(replyFinished(QNetworkReply*))) ) {

        QDEBUG() << "http request finished connection false";

    }


    QIcon mIcon("://icons/ic_cloud_done_black_24dp_2x.png");
    setWindowTitle("Packet Sender Cloud");
    setWindowIcon(mIcon);

    setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);


    QSettings settings(SETTINGSFILE, QSettings::IniFormat);

    ui->usernameEdit->setText(settings.value("cloudUsername", "").toString());
    ui->passwordEdit->setText(settings.value("cloudPassword", "").toString());
    ui->rememberLoginCheck->setChecked(settings.value("rememberLoginCheck", false).toBool());

    ui->usernameEdit->setFocus();


    packets = Packet::fetchAllfromDB("");

    ui->shareBlurbLabel->setText("Saving " + QString::number(packets.size()) +" packet set to cloud");


    settings.setValue("cloudPassword", ui->passwordEdit->text());

}

CloudUI::~CloudUI()
{
    delete ui;
}

QString getpw64(QString pw)
{
    return QString(pw.toLatin1().toBase64());
}

void popMsg(QString title, QString msg, bool isError)
{

    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setStandardButtons(QMessageBox::Ok );
    msgBox.setDefaultButton(QMessageBox::Ok);
    if(isError){
        msgBox.setIcon(QMessageBox::Warning);
    } else {
        msgBox.setIcon(QMessageBox::Information);

    }
    msgBox.setText(msg);
    msgBox.exec();

}

bool isLetterNumUnder(QString str)
{

    for (int i =0;i<str.size();i++)
    {
        if(str[i] == '_') {
            continue;
        }
        if (str[i].isDigit()) {
            continue;
        }
        if (str[i].isLetter())  {
            continue;
        }

        return false;
    }

    return true;
}


void CloudUI::replyFinished(QNetworkReply* request)
{
    QByteArray data = request->readAll();
    QString dataString = QString(data).trimmed();
    QDEBUG() <<"Request complete:" << data.size();

    ui->loginButton->setEnabled(true);
    ui->importURLButton->setEnabled(true);

    if(dataString.toLower().startsWith("success")) {
        popMsg("Success", dataString, false);
        if(ui->passwordConfirmEdit->isVisible()) {
            ui->cloudTabWidget->setCurrentIndex(2);
            on_createAccountButton_clicked();
        }
        return;
    }

    if(dataString.toLower().startsWith("error")) {
        popMsg("Error", dataString, true);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);



    bool success = false;

    if(!doc.isNull()) {
        //valid json
        if(doc.isArray()) {
            //valid array
            QJsonArray jsonArray = doc.array();
            if(!jsonArray.isEmpty()) {
                QDEBUG() << "Found" <<  jsonArray.size() << "sets";
                packetSets.clear();

                for(int i=0; i < jsonArray.size(); i++) {
                    PacketSet pktSet;
                    QJsonObject json = jsonArray[i].toObject();
                    pktSet.description = json["description"].toString();
                    QByteArray jsonData = json["packetjson"].toString().toLatin1();
                    if(!jsonData.isEmpty()) {
                        pktSet.packets = Packet::ImportJSON(jsonData);
                        QDEBUG() << "Set" << pktSet.description << "has" << pktSet.packets.size() << "packets";

                        if(pktSet.packets.size() > 0) {
                            success = true;
                            packetSets.append(pktSet);
                        }
                    } else {
                        QDEBUG() << "Packet array is null";
                    }

                }

            }
        }
    }

    if(success) {
        popMsg("Success", "Found " + QString::number(packetSets.size()) + " sets of packets!", false);
        loadPacketSetTable();
        ui->cloudTabWidget->setCurrentIndex(1);


    } else {
        popMsg("Error", "Did not fetch any packets", true);

    }


}


void CloudUI::loadPacketSetTable()
{
    ui->packetSetTable->clear();

    QStringList tableHeaders;
    tableHeaders << "Description" << "Packet Count";


    ui->packetSetTable->setColumnCount(2);

    ui->packetSetTable->verticalHeader()->show();
    ui->packetSetTable->horizontalHeader()->show();
    ui->packetSetTable->setHorizontalHeaderLabels(tableHeaders);


    ui->packetSetTable->setRowCount(packetSets.size());
    for(int i=0; i < packetSets.size(); i++ ) {
        QTableWidgetItem  * itemDescription = new QTableWidgetItem(packetSets[i].description);
        QTableWidgetItem  * itemSize = new QTableWidgetItem(QString::number(packetSets[i].packets.size()));

        itemDescription->setData(Qt::UserRole, i); //set index
        itemSize->setData(Qt::UserRole, i); //set index

        ui->packetSetTable->setItem(i, 0, itemDescription);
        ui->packetSetTable->setItem(i, 1, itemSize);
    }


    ui->packetSetTable->setSortingEnabled(true);

    ui->packetSetTable->resizeColumnsToContents();
    ui->packetSetTable->resizeRowsToContents();
    ui->packetSetTable->horizontalHeader()->setStretchLastSection( true );


}



void CloudUI::on_loginButton_clicked()
{

    QUrlQuery postData;

    QString un = ui->usernameEdit->text().trimmed();

    if(!isLetterNumUnder(un)) {
        popMsg("Invalid.", "Usernames may only be letters, numbers, underscores", true);
        ui->usernameEdit->setFocus();
        return;
    }

    if(un.size() < 3) {
        popMsg("Too short.", "Username must be at least 3 characters.", true);
        ui->usernameEdit->setFocus();
        return;
    }

    QString pw = ui->passwordEdit->text().trimmed();
    if(pw.size() < 3) {
        popMsg("Too short.", "Passwords must be at least 3 characters.", true);
        ui->usernameEdit->setFocus();
        return;
    }


    QString pw64 = getpw64(ui->passwordEdit->text());
    postData.addQueryItem("un", ui->usernameEdit->text());
    postData.addQueryItem("pw64", pw64);

    if(ui->passwordConfirmEdit->isVisible()) {
        if(ui->passwordConfirmEdit->text() != ui->passwordEdit->text()) {
            popMsg("Mismatch.", "Passwords do not match.", true);
            ui->passwordEdit->setFocus();
            return;
        }
        postData.addQueryItem("newaccount", "1");
    }


    ui->loginButton->setEnabled(false);


    QSettings settings(SETTINGSFILE, QSettings::IniFormat);
    settings.setValue("rememberLoginCheck", ui->rememberLoginCheck->isChecked());


    if(ui->rememberLoginCheck->isChecked()) {
        settings.setValue("cloudUsername", ui->usernameEdit->text());
        settings.setValue("cloudPassword", ui->passwordEdit->text());
    }

    doPost(postData);

}


void CloudUI::doPost(QUrlQuery postData)
{

    QNetworkRequest request(QUrl(CLOUD_URL));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
        "application/x-www-form-urlencoded");
    QByteArray pData = postData.toString(QUrl::FullyEncoded).toUtf8();
    QDEBUG() << (CLOUD_URL) << QString(pData);
    http->post(request, pData);

}

void CloudUI::on_saveToCloudButton_clicked()
{
    QDEBUG();
    QUrlQuery postData;
    QString pw64 = getpw64(ui->passwordEdit->text());
    postData.addQueryItem("un", ui->usernameEdit->text());
    postData.addQueryItem("pw64", pw64);

    QString pname = ui->packetSetNameEdit->text().trimmed();

    if(pname.isEmpty()) {
        popMsg("Empty", "Set name cannot be empty", true);
        return;
    }

    postData.addQueryItem("setname", pname);

    if(ui->makePublicCheck->isChecked()) {
        postData.addQueryItem("makepublic", "1");

        QString pubblurb = ui->descriptionExportEdit->toPlainText().trimmed();

        if(pubblurb.isEmpty()) {
            popMsg("Empty", "Public description cannot be empty", true);
            return;
        }

        postData.addQueryItem("pubblurb", pubblurb);

    }

    doPost(postData);


}

void CloudUI::on_viewPublicButton_clicked()
{
    QDEBUG();

}

void CloudUI::on_importURLButton_clicked()
{
    QString url = ui->importURLEdit->text().trimmed();
    url.replace("=", "/");  //key is the last part of URL, whether clean or key

    QStringList findkey = url.split("/");
    if(findkey.size() > 0) {

        ui->importURLButton->setEnabled(false);

        QString url = findkey.last();
        QString requestURL = CLOUD_URL + QString("?key=") + url;
        QDEBUGVAR(requestURL);

        http->get(QNetworkRequest(QUrl(requestURL)));

    }


}

void CloudUI::on_importPacketsButton_clicked()
{

    QModelIndexList indexes = ui->packetSetTable->selectionModel()->selectedIndexes();
    QDEBUGVAR(indexes.size());

    if(indexes.size() > 0) {
        QModelIndex index = indexes.first();
        int packetsetindex = index.data(Qt::UserRole).toInt();
        QDEBUGVAR(packetsetindex);
        if(packetSets.size() > packetsetindex) {
            emit packetsImported(packetSets[packetsetindex].packets);
        }
    }


}



void CloudUI::on_makePublicCheck_clicked(bool checked)
{
    ui->descriptionExportEdit->setEnabled(checked);
}

void CloudUI::on_createAccountButton_clicked()
{
    if(ui->passwordConfirmEdit->isVisible()) {
        ui->passwordConfirmEdit->hide();
        ui->passwordConfirmLabel->hide();
        ui->createAccountButton->setText("Create a new account.");
        ui->loginButton->setText("Login");
    } else {
        ui->passwordConfirmEdit->show();
        ui->passwordConfirmLabel->show();
        ui->createAccountButton->setText("Login instead.");
        ui->loginButton->setText("Sign-up");
    }

}
