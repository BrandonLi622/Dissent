#include "SFTMessageManager.hpp"
#include <QDebug>

//TODO: Need to check on roundPhase
//TODO: I'm assuming that GetId() gives me this current ID, though there is also GetLocalId
//TODO: Still need to tell when the collection period is over


namespace Dissent{
namespace SFT {
SFTMessageManager::SFTMessageManager(SFT::SFTNullRound *round)
{
    this->round = round;
    this->startRound();
}

void SFTMessageManager::startRound()
{
    this->roundPhase = SFT::SFTNullRound::RoundPhase::Collection;
    this->receivedClientMsgs = new QHash<int, QByteArray>();
    this->receivedServerCiphers = new QHash<int, QByteArray>();
    this->respondedClients = new QList<int>();
    this->respondedServers = new QList<int>();
    this->totalRoundClients = new QList<int>();
}

void SFTMessageManager::startClientAttendance()
{
    this->respondedServers->clear();
    this->respondedServers->append(this->round->GetLocalId().GetInteger().GetInt32());
    QList<QVariant> serializedClientList = QList<QVariant>();

    for (int i = 0; i < this->respondedClients->count(); i++)
    {
        int id = this->respondedClients->at(i);
        this->totalRoundClients->append(id);
        serializedClientList.append(QVariant(id));
    }
    this->roundPhase = SFT::SFTNullRound::RoundPhase::ClientAttendance;
    QVariantMap map = QVariantMap();
    map.insert("Type", this->roundPhase);
    map.insert("ClientList", QVariant(serializedClientList));
    this->round->broadcastToServers(map);
}

void SFTMessageManager::startCipherExchange()
{
    this->roundPhase = SFT::SFTNullRound::RoundPhase::ExchangeCiphers;
    QByteArray cipherText = this->createCipher();
    QVariantMap map = QVariantMap();
    map.insert("Type", MessageTypes::ServerCipher);
    map.insert("Cipher", cipherText);
    this->round->broadcastToServers(map);
}

void SFTMessageManager::startViewVoting()
{
    this->roundPhase = SFT::SFTNullRound::RoundPhase::ViewChangeVoting;
}

QVariantMap SFTMessageManager::genViewChangeProposal(int viewNum)
{
    QVariantMap map = QVariantMap();
    map.insert("Type", MessageTypes::ViewChangeVote);
    map.insert("ViewNum", viewNum);
    return map;
}

QVariantMap SFTMessageManager::genClientViewChangeNotification(int viewNum)
{
    QVariantMap map = QVariantMap();
    map.insert("Type", MessageTypes::ViewChangeNotification);
    map.insert("ViewNum", viewNum);
    return map;
}

QVariantMap SFTMessageManager::genDecryption(int viewNum)
{
    QByteArray decrypted = getDecrypted();
    QVariantMap map;
    map.insert("Type", MessageTypes::Decrypted);
    map.insert("Decrypted", decrypted);
    map.insert("ViewNum", viewNum);
    return map;
}

QByteArray SFTMessageManager::encryptionKey(int serverID, int clientID, int keyLength, int numClients)
{
    qDebug() << "ID's are " << serverID << clientID;

    QByteArray key;
    //TODO: Making an assumption about the number of clients and servers
    srand(serverID * 100 + clientID);
    for (int i = 0; i < numClients; i++)
    {
        for (int j = 0; j < keyLength; j++)
        {
            char num = char(32 + rand() % 96); //Just for testing purposes make them all printable characters
            key.append(num); // Pick a random character
        }
    }
    return key;
}

//TODO: What does keyLength actually mean? the length of each slot?
QByteArray SFTMessageManager::insertInSlot(QString msg)
{
    //So far message only contains my own slot
    QByteArray fullMsg;
    for (int i = 0; i < this->keyLength; i++)
    {
        //null values for other people's
        for (int j = 0; j < this->keyLength; j++)
        {
            fullMsg.append(char(0));
        }
    }

    fullMsg.append(msg); //Add in my message;

    //The index of this persons slot
    int slotNum = this->round->GetClients().GetIndex(this->round->GetLocalId());

    for (int i = slotNum + 1; i < this->round->GetClients().Count(); i++)
    {
        //null values for other people's
        for (int j = 0; j < this->keyLength; j++)
        {
            fullMsg.append(char(0));
        }
    }

    QByteArray testMsg;
    testMsg.append(fullMsg);
    qDebug() << "Full msg: " << testMsg.replace('\0', ' ');

    return fullMsg;
}

QByteArray SFTMessageManager::encryptClientMessage(int clientID, QByteArray data)
{
    QByteArray xored = data;
    for (int i = 0; i < this->round->GetServers().Count(); i++)
    {
        int serverID = this->round->GetServers().GetId(i).GetInteger().GetInt32();
        QByteArray key = encryptionKey(serverID, clientID, this->keyLength, this->round->GetServers().Count());
        xored = xorBytes(xored, key);
    }
    return xored;
}

//Utility
QByteArray SFTMessageManager::xorBytes(QByteArray a1, QByteArray a2)
{
    //Assuming strings are the same length
    QByteArray xored;

    if (a1.length() != a2.length())
    {
        qDebug() << "test";
    }

    for (int i = 0; i < a1.length(); i++)
    {
        xored.append(a1.at(i) ^ a2.at(i));
    }
    return xored;
}

//Pass in the client's combined text
//Should only call this if we have at least one client, am I right?
QByteArray SFTMessageManager::createCipher()
{
    qDebug() << "All messages";
    qDebug() << *receivedClientMsgs;

    //Xor's the bits from the client first
    int id = receivedClientMsgs->keys().value(0);
    QByteArray xored = receivedClientMsgs->value(id);
    for (int i = 1; i < receivedClientMsgs->keys().length(); i++)
    {
        id = receivedClientMsgs->keys().value(i);
        xored = xorBytes(xored, receivedClientMsgs->value(id));
    }

    int thisID = this->round->GetLocalId().GetInteger().GetInt32();

    //Assuming fixed length messages
    //Gets the encryption key for all of the clients
    for (int i = 0; i < totalRoundClients->count(); i++)
    {
        int clientID = totalRoundClients->at(i);
        QByteArray key = encryptionKey(thisID, clientID, this->round->messageLength, int(this->round->GetClients().Count()));

        xored = xorBytes(xored, key);
    }
    return xored;
}

QByteArray SFTMessageManager::getDecrypted()
{
    //Here it assumes that we have all of the ciphers
    QList<QByteArray> allCiphers = receivedServerCiphers->values();
    QByteArray decrypted = allCiphers.value(0);

    for (int i = 1; i < allCiphers.length(); i++)
    {
        decrypted = xorBytes(decrypted, allCiphers.value(i));
    }

    return decrypted;
}

void SFTMessageManager::switchServerMessage(QVariantMap map, const Connections::Id &from)
{
    qDebug() << from;

    if (!map.keys().contains("Type"))
    {
        qDebug() << "Dropping bad message" << map;
        return;
    }

    if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ClientMessage)
    {
        processClientMessage(map, from);
    }
    else if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ViewChangeVote)
    {
        processViewChangeProposal(map, from);
    }
    else if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ServerCipher)
    {
        processCipher(map, from);
    }
    else
    {
        //IT SHOULD NEVER REACH HERE. If it gets here then we don't do anything
        qDebug() << "Malformed message";
    }
}

void SFTMessageManager::switchClientMessage(QVariantMap map, const Connections::Id &from)
{
    qDebug() << from;

    if (!map.keys().contains("Type"))
    {
        qDebug() << "Dropping bad message" << map;
        return;
    }

    if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ViewChangeNotification)
    {
        qDebug() << "hello";
    }
    else
    {
        //IT SHOULD NEVER REACH HERE. If it gets here then we don't do anything
        qDebug() << "Malformed message";
    }
}

void SFTMessageManager::processCipher(QVariantMap map, const Connections::Id &from)
{
    if (!this->round->sftViewManager->inCurrentView(from))
    {
        qDebug() << "Ignore all ciphertexts from people not in the round";
    }

    int serverID = from.GetInteger().GetInt32();

    QByteArray cipher = map.value("Cipher").toByteArray();
    receivedServerCiphers->insert(serverID, cipher);

    if (!respondedServers->contains(serverID))
    {
        respondedServers->append(serverID);
    }
    qDebug() << "Received cipher";

    if (respondedServers->length() == this->round->sftViewManager->getViewSize())
    {
        //DONE WITH THE ROUND!
        //Should now decrypt and send back to clients
        QVariantMap map = genDecryption(this->round->sftViewManager->getCurrentViewNum());
        this->round->broadcastToClients(map);
        this->startRound();  //TODO: This is going to be a problem for synchronization...
    }
    /*
    if (serverWait->empty() && roundPhase == 1 && serverIDs->contains(thisID))
    {
    }*/
}


void SFTMessageManager::processViewChangeProposal(QVariantMap map, const Connections::Id &from)
{
    this->roundPhase = SFT::SFTNullRound::RoundPhase::ViewChangeVoting;

    //Also make sure not to double count per server!!! (how do I do that?)
    int viewNum = map.value("ViewNum").toInt();

    //Means the view changed
    int viewChangeCode = this->round->sftViewManager->addViewChangeVote(viewNum, from);
    if (viewChangeCode > -1)
    {
        //Should notify clients somehow
        QVariantMap map = this->genClientViewChangeNotification(viewChangeCode);
        this->round->broadcastToClients(map);
        this->startRound();
    }
}

void SFTMessageManager::processClientList(QVariantMap map, const Connections::Id &from)
{
    QList<QVariant> clientList = map.value("ClientList").toList();
    for (int i = 0; i < clientList.count(); i++)
    {
        if (!this->totalRoundClients->contains(clientList.at(i).toInt()))
        {
            this->totalRoundClients->append(clientList.at(i).toInt());
        }
    }
    this->respondedServers->append(from.GetInteger().GetInt32());

    if (this->respondedServers->count() == this->round->GetServers().Count())
    {
        //Then we've got all of the clients
    }
}

void SFTMessageManager::processClientMessage(QVariantMap map, const Connections::Id &from)
{
    if (this->roundPhase != SFT::SFTNullRound::RoundPhase::Collection)
    {
        qDebug() << "Cannot collect messages anymore";
        return;
    }

    int viewNum = map.value("ViewNum").toUInt();
    QByteArray msg = map.value("Message").toByteArray();

    int clientID = from.GetInteger().GetInt32();
    receivedClientMsgs->insert(clientID, msg);

    if (this->round->sftViewManager->getCurrentViewNum() != viewNum)
    {
        qDebug() << "Wrong view number!";
        return;
    }

    if (!this->respondedClients->contains(clientID))
    {
        this->respondedClients->append(clientID);
    }
}




}
}
