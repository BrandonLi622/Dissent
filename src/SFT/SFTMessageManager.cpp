#include "SFTMessageManager.hpp"
#include <QDebug>

//TODO: Need to check on roundPhase
//TODO: I'm assuming that GetId() gives me this current ID, though there is also GetLocalId
//TODO: Still need to tell when the collection period is over


namespace Dissent{
namespace SFT {
SFTMessageManager::SFTMessageManager(Connections::Id localId,
                                     const Identity::Roster &clients,
                                     const Identity::Roster &servers,
                                     SFT::SFTViewManager *sftViewManager)
//SFTMessageManager::SFTMessageManager(SFT::SFTNullRound *round)
{
    this->localId = localId;
    this->m_clients = clients;
    this->m_servers = servers;
    this->sftViewManager = sftViewManager;
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
    this->respondedServers->append(this->localId.GetInteger().GetInt32());
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

    emit this->broadcastToServers(map);
}

void SFTMessageManager::startCipherExchange()
{
    this->roundPhase = SFT::SFTNullRound::RoundPhase::ExchangeCiphers;
    QByteArray cipherText = this->createCipher();
    QVariantMap map = QVariantMap();
    map.insert("Type", MessageTypes::ServerCipher);
    map.insert("Cipher", cipherText);

    emit this->broadcastToServers(map);
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

QVariantMap SFTMessageManager::genClientMessage(qint32 viewNum, QByteArray data)
{
    QVariantMap map;
    map.insert("Type", MessageTypes::ClientMessage);
    map.insert("Message", data);
    map.insert("ViewNum", viewNum);
    return map;
}

void SFTMessageManager::sendViewChangeProposal(qint32 viewNum)
{
    QVariantMap map = genViewChangeProposal(viewNum);
    emit this->broadcastToServers(map);
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
    int slotNum = this->m_clients.GetIndex(this->localId);

    for (int i = slotNum + 1; i < this->m_clients.Count(); i++)
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
    QVector<Connections::Id> servers = this->sftViewManager->getCurrentServers();
    //Should only encrypt based on who is actually in the round!!
    for (int i = 0; i < servers.count(); i++)
    {
        int serverID = servers.at(i).GetInteger().GetInt32();
        //This should really be the number of clients * keylength
        QByteArray key = encryptionKey(serverID, clientID, this->m_clients.Count() * this->keyLength, servers.count());
        xored = xorBytes(xored, key);
    }
    return xored;
}

void SFTMessageManager::clientRoundMessageSend(QString msg)
{
    QString sub = msg.mid(0, this->keyLength); //Truncate the message to what we can actually send
    QByteArray insertedMsg = insertInSlot(sub);
    QByteArray encrypted = encryptClientMessage(this->localId.GetInteger().GetInt32(), insertedMsg);
    QVariantMap map = genClientMessage(this->sftViewManager->getCurrentViewNum(), encrypted); //TODO: Better hope this value is correct

    //Right now it just picks a random server to send to
    srand (time(NULL));
    int random = rand() % this->sftViewManager->getViewSize();
    Connections::Id server = this->sftViewManager->getCurrentServers().at(random);

    emit this->sendToSingleServer(server, map);
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

    int thisID = this->localId.GetInteger().GetInt32();

    //Assuming fixed length messages
    //Gets the encryption key for all of the clients
    for (int i = 0; i < totalRoundClients->count(); i++)
    {
        int clientID = totalRoundClients->at(i);
        QByteArray key = encryptionKey(thisID, clientID, this->keyLength, int(this->m_clients.Count()));

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
    qDebug() << "Receiving message at server from " << from;
    qDebug() << map;

    if (!map.keys().contains("Type"))
    {
        qDebug() << "Dropping bad message" << map;
        return;
    }

    if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ClientMessage)
    {
        qDebug() << "Received a client message";
        processClientMessage(map, from);
    }
    else if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ViewChangeVote)
    {
        qDebug() << "Received a view change vote message";
        processViewChangeProposal(map, from);
    }
    else if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ClientAttendance)
    {
        qDebug() << "Received a client attendance message";
        processClientList(map, from);
    }
    else if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ServerCipher)
    {
        qDebug() << "Received a server cipher message";
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
    qDebug() << "Receiving message at client from " << from;
    qDebug() << map;

    if (!map.keys().contains("Type"))
    {
        qDebug() << "Dropping bad message" << map;
        return;
    }

    if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ViewChangeNotification)
    {
        qDebug() << "Received a view change notification message";
        processViewChangeNotification(map, from);
    }

    else if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::Decrypted)
    {
        qDebug() << "Received the decrypted message";
        //TODO: Need to figure out what to do here
        processDecrypted(map);
    }
    else
    {
        //IT SHOULD NEVER REACH HERE. If it gets here then we don't do anything
        qDebug() << "Malformed message";
    }
}

void SFTMessageManager::processCipher(QVariantMap map, const Connections::Id &from)
{
    if (!this->sftViewManager->inCurrentView(from))
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

    if (respondedServers->length() == this->sftViewManager->getViewSize())
    {
        //DONE WITH THE ROUND!
        //Should now decrypt and send back to clients
        QVariantMap map = genDecryption(this->sftViewManager->getCurrentViewNum());
        emit this->broadcastToClients(map);
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
    int viewChangeCode = this->sftViewManager->addViewChangeVote(viewNum, from);
    if (viewChangeCode > -1)
    {
        //Should notify clients somehow
        QVariantMap map = this->genClientViewChangeNotification(viewChangeCode);

        emit this->broadcastToClients(map);
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

    if (this->respondedServers->count() == this->m_servers.Count())
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

    int viewNum = map.value("ViewNum").toInt();
    QByteArray msg = map.value("Message").toByteArray();

    int clientID = from.GetInteger().GetInt32();
    receivedClientMsgs->insert(clientID, msg);

    if (this->sftViewManager->getCurrentViewNum() != viewNum)
    {
        qDebug() << "Wrong view number!";
        return;
    }

    if (!this->respondedClients->contains(clientID))
    {
        this->respondedClients->append(clientID);
    }
}

void SFTMessageManager::processViewChangeNotification(QVariantMap map, const Connections::Id &from)
{
    if (!this->m_servers.Contains(from))
    {
        qDebug() << "View change notification invalid. Not from a server in the round";
        return;
    }

    int viewNum = map.value("ViewNum").toInt();
    this->sftViewManager->setNewView(viewNum);
}

void SFTMessageManager::processDecrypted(QVariantMap map)
{
    QByteArray b = map.value("Decrypted").toByteArray();
    QString finalOutput = QString(b);
    qDebug() << map << b << finalOutput;
}




}
}
