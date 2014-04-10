#include "SFTMessageManager.hpp"
#include <QDebug>

//TODO: Need to check on roundPhase
//TODO: I'm assuming that GetId() gives me this current ID, though there is also GetLocalId
//TODO: Still need to tell when the collection period is over

//TODO: Servers that don't receive anything still must send their ciphertexts
//TODO: There's a bug causing not all of the attendance things to be received


namespace Dissent{
namespace SFT {
SFTMessageManager::SFTMessageManager(Connections::Id localId,
                                     const Identity::Roster &clients,
                                     const Identity::Roster &servers,
                                     QList<Connections::Id> connections,
                                     bool isServer,
                                     SFT::SFTViewManager *sftViewManager)
//SFTMessageManager::SFTMessageManager(SFT::SFTNullRound *round)
{
    this->localId = localId;
    this->m_clients = clients;
    this->m_servers = servers;
    this->m_connections = connections;
    this->isServer = isServer;
    this->sftViewManager = sftViewManager;

    this->receivedClientMsgs = new QHash<int, QByteArray>();
    this->receivedServerCiphers = new QHash<int, QByteArray>();
    this->respondedClients = new QList<int>();
    this->respondedServers = new QList<int>();
    this->totalRoundClients = new QList<int>();

    this->startRound();
}

QList<QVariant> SFTMessageManager::getReceivedClients()
{
    QList<QVariant> serializedClientList = QList<QVariant>();

    for (int i = 0; i < this->respondedClients->count(); i++)
    {
        int id = this->respondedClients->at(i);
        serializedClientList.append(QVariant(id));
    }
    return serializedClientList;
}

//TODO: Perhaps this is better named startMessageExchange()
void SFTMessageManager::startRound()
{
    qDebug() << "\n\n\nStarting Round" << this->localId << "\n\n\n";

    this->roundPhase = SFT::SFTNullRound::RoundPhase::CollectionPhase;
    this->receivedClientMsgs->clear();
    this->receivedServerCiphers->clear();
    this->respondedClients->clear();
    this->respondedServers->clear();
    this->totalRoundClients->clear();
}

void SFTMessageManager::startClientAttendance()
{
    qDebug() << "\n\n\nStarting Client Attendance" << this->localId << "\n\n\n";

    this->respondedServers->clear();
    this->respondedServers->append(this->localId.GetInteger().GetInt32());
    for (int i = 0; i < this->respondedClients->count(); i++)
    {
        int id = this->respondedClients->at(i);
        this->totalRoundClients->append(id);
    }
    qDebug() << "Total round clients are: " << *totalRoundClients;
    QList<QVariant> serializedClientList = getReceivedClients();
    this->roundPhase = SFT::SFTNullRound::RoundPhase::ClientAttendancePhase;
    QVariantMap map = QVariantMap();
    map.insert("Type", MessageTypes::ClientAttendance);
    map.insert("ClientList", QVariant(serializedClientList));

    emit this->broadcastToServers(map);
    qDebug() << "Sending ClientAttendance request" << this->localId;
    sendRequest(SFT::SFTNullRound::RoundPhase::ClientAttendancePhase);

    //Need to ask for attendance lists from other servers

}

void SFTMessageManager::startCipherExchange()
{
    qDebug() << "\n\n\nStarting Cipher Exchange" << this->localId << "\n\n\n";

    this->receivedServerCiphers->clear();
    this->respondedServers->clear();
    this->respondedServers->append(this->localId.GetInteger().GetInt32());
    this->roundPhase = SFT::SFTNullRound::RoundPhase::ExchangeCiphersPhase;
    QByteArray cipherText = this->createCipher();
    QVariantMap map = QVariantMap();
    map.insert("Type", MessageTypes::ServerCipher);
    map.insert("Cipher", cipherText);
    this->receivedServerCiphers->insert(this->localId.GetInteger().GetInt32(), cipherText);

    emit this->broadcastToServers(map);
    sendRequest(SFT::SFTNullRound::RoundPhase::ExchangeCiphersPhase);
}

void SFTMessageManager::startViewVoting()
{
    qDebug() << "\n\n\nStarting View Voting" << this->localId << "\n\n\n";

    this->roundPhase = SFT::SFTNullRound::RoundPhase::ViewChangeVotingPhase;
    int newView = this->sftViewManager->nextGoodView(this->sftViewManager->getCurrentViewNum() + 1);
    this->sftViewManager->proposeNewView(newView);
    this->sftViewManager->addViewChangeVote(newView, true, this->localId); //Remember to vote for myself
    sendViewChangeProposal(newView);
    sendRequest(SFT::SFTNullRound::RoundPhase::ViewChangeVotingPhase);
}

QVariantMap SFTMessageManager::genViewChangeProposal(int viewNum)
{
    QVariantMap map = QVariantMap();
    map.insert("Type", QVariant(MessageTypes::ViewChangeVote));
    map.insert("ViewNum", QVariant(viewNum));
    map.insert("Vote", true);
    return map;
}

QVariantMap SFTMessageManager::genClientViewChangeNotification(int viewNum)
{
    QVariantMap map = QVariantMap();
    map.insert("Type", QVariant(MessageTypes::ViewChangeNotification));
    map.insert("ViewNum", QVariant(viewNum));
    return map;
}

QVariantMap SFTMessageManager::genDecryption(int viewNum)
{
    QByteArray decrypted = getDecrypted();
    QVariantMap map;
    map.insert("Type", QVariant(MessageTypes::Decrypted));
    map.insert("Decrypted", QVariant(decrypted));
    map.insert("ViewNum", QVariant(viewNum));
    return map;
}

QVariantMap SFTMessageManager::genClientMessage(int viewNum, QByteArray data)
{
    QVariantMap map;
    map.insert("Type", QVariant(MessageTypes::ClientMessage));
    map.insert("Message", QVariant(data));
    map.insert("ViewNum", QVariant(viewNum));
    return map;
}

QVariantMap SFTMessageManager::genRequest(int type)
{
    QVariantMap map;
    map.insert("Type", QVariant(MessageTypes::RequestInfo));
    map.insert("Request", QVariant(type));
    return map;
}

QVariantMap SFTMessageManager::genInputRequest()
{
    QVariantMap map;
    map.insert("Type", QVariant(MessageTypes::RequestInput));
    return map;
}

void SFTMessageManager::sendViewChangeProposal(int viewNum)
{
    QVariantMap map = genViewChangeProposal(viewNum);
    emit this->broadcastToServers(map);
}

void SFTMessageManager::sendRequest(int type)
{
    QVariantMap map = genRequest(type);
    emit this->broadcastToServers(map);
}

void SFTMessageManager::sendInputRequest()
{
    QVariantMap map = genInputRequest();
    emit this->broadcastToDownstreamClients(map);
}

QByteArray SFTMessageManager::encryptionKey(int serverID, int clientID, int keyLength, int numClients)
{
    qDebug() << "ID's are " << serverID << clientID;

    QByteArray key;
    //TODO: These are unique, right?
    //The 100 is just for more variability
    srand((serverID * numClients + clientID) * 100);
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
    int index = this->m_clients.GetIndex(this->localId);
    for (int i = 0; i < index; i++)
    {
        //null values for other people's
        for (int j = 0; j < this->keyLength; j++)
        {
            fullMsg.append(char(0));
        }
    }

    fullMsg.append(msg); //Add in my message;

    for (int i = index + 1; i < this->m_clients.Count(); i++)
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

    qDebug() << "Client knows: " << servers.count() << "servers";
    for (int i = 0; i < servers.count(); i++)
    {
        qDebug() << "Before xor: " << xored;

        int serverID = servers.at(i).GetInteger().GetInt32();
        //This should really be the number of clients * keylength
        QByteArray key = encryptionKey(serverID, clientID, this->keyLength, this->m_clients.Count());
        xored = xorBytes(xored, key);

        qDebug() << "After xor: " << xored;
    }
    qDebug() << "Client's message length: " << xored.length();

    return xored;
}

void SFTMessageManager::clientRoundMessageSend(QString msg)
{
    QString sub = msg.mid(0, this->keyLength); //Truncate the message to what we can actually send
    QByteArray insertedMsg = insertInSlot(sub);
    QByteArray encrypted = encryptClientMessage(this->localId.GetInteger().GetInt32(), insertedMsg);
    QVariantMap map = genClientMessage(this->sftViewManager->getCurrentViewNum(), encrypted); //TODO: Better hope this value is correct

    qDebug() << "Client is sending: " << msg << map << " to";
    qDebug() << this->m_connections;

    //Should only have a single entry!
    emit this->sendToSingleNode(this->m_connections.at(0), map);
}

//Utility
QByteArray SFTMessageManager::xorBytes(QByteArray a1, QByteArray a2)
{
    qDebug() << "Xoring bytes";

    //Assuming strings are the same length
    QByteArray xored;

    if (a1.length() != a2.length())
    {
        qDebug() << a1.length() << "!=" << a2.length();
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
    qDebug() << "All direct messages";
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
    qDebug() << totalRoundClients->count() << " clients" << *totalRoundClients;
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
    qDebug() << "All ciphers: " << allCiphers;
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

    //Do this elsewhere!
    /*
    if (!this->sftViewManager->inCurrentView(this->localId))
    {
        //TODO: Server should pass on messages if it can
        qDebug() << "This server is not in the current view";
        return;
    }*/


    if (!map.keys().contains("Type"))
    {
        qDebug() << "Dropping bad message" << map;
        return;
    }

    if ((MessageTypes) map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ClientMessage)
    {
        qDebug() << "Received a client message";
        processClientMessage(map, from);
    }
    else if ((MessageTypes) map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ViewChangeVote)
    {
        qDebug() << "Received a view change vote message";
        processViewChangeProposal(map, from);
    }
    else if ((MessageTypes) map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ClientAttendance)
    {
        qDebug() << "Received a client attendance message";
        processClientList(map, from);
    }
    else if ((MessageTypes) map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ServerCipher)
    {
        qDebug() << "Received a server cipher message";
        processCipher(map, from);
    }
    else if ((MessageTypes) map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::RequestInfo)
    {
        qDebug() << "Received an info request message";
        processInfoRequest(map, from);
    }
    else
    {
        //IT SHOULD NEVER REACH HERE. If it gets here then we don't do anything
        qDebug() << "Malformed message, received by server";
        qDebug() << map;
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
        processDecrypted(map);
    }

    else if (map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::RequestInput)
    {
        qDebug() << "Received the client input message";
        processInputRequest(map, from);
    }

    else
    {
        //IT SHOULD NEVER REACH HERE. If it gets here then we don't do anything
        qDebug() << "Malformed message, received by client";
    }
}

void SFTMessageManager::processCipher(QVariantMap map, const Connections::Id &from)
{
    if (this->roundPhase != SFT::SFTNullRound::RoundPhase::ExchangeCiphersPhase)
    {
        qDebug() << "Wrong time for cipher texts" << this->roundPhase;
        return;
    }

    if (!this->sftViewManager->inCurrentView(from))
    {
        qDebug() << "Sending server is not in current view, ignoring ciphertext";
        return;
    }

    if (this->respondedServers->contains(from.GetInteger().GetInt32()))
    {
        qDebug() << "Already received a message from this server";
        return;
    }

    qDebug() << "Expecting " << this->sftViewManager->getViewSize() << " cipher responses";
    qDebug() << "Received " << this->receivedServerCiphers->count() + 1 << " cipher responses";
    int serverID = from.GetInteger().GetInt32();

    QByteArray cipher = map.value("Cipher").toByteArray();
    receivedServerCiphers->insert(serverID, cipher);

    if (!respondedServers->contains(serverID))
    {
        respondedServers->append(serverID);
    }
    qDebug() << "Received cipher";

    if (receivedServerCiphers->count() == this->sftViewManager->getViewSize())
    {
        //DONE WITH THE ROUND!
        //Should now decrypt and send back to clients
        QVariantMap map = genDecryption(this->sftViewManager->getCurrentViewNum());
        //TODO: Should really only be sending to own clients...
        emit this->broadcastToDownstreamClients(map);
        this->startRound();  //TODO: This is going to be a problem for synchronization...
    }
}

void SFTMessageManager::processInputRequest(QVariantMap map, const Connections::Id &from)
{
    SFT::SFTNullRound::RoundPhase type = (SFT::SFTNullRound::RoundPhase) map.value("Request").toInt();
    if (type == SFT::SFTNullRound::RoundPhase::CollectionPhase)
    {
        qDebug() << "Received input request" << this->localId << "from " << from;

        //Client doesn't really have phases they care about
        //this->clientRoundMessageSend("def"); //TODO: Remove this
        return;
    }
}

void SFTMessageManager::processInfoRequest(QVariantMap map, const Connections::Id &from)
{
    SFT::SFTNullRound::RoundPhase type = (SFT::SFTNullRound::RoundPhase) map.value("Request").toInt();

    if (type == SFT::SFTNullRound::RoundPhase::ExchangeCiphersPhase)
    {
        qDebug() << "Received ciphers request" << this->localId << "";

        if (this->roundPhase >= SFT::SFTNullRound::RoundPhase::ExchangeCiphersPhase)
        {
            qDebug() << "Responding to ciphers request" << this->localId << "";

            QByteArray cipherText = this->createCipher();
            QVariantMap resp = QVariantMap();
            resp.insert("Type", QVariant(MessageTypes::ServerCipher));
            resp.insert("Cipher", QVariant(cipherText));

            emit this->sendToSingleNode(from, resp);
            return;
        }
    }
    else if (type == SFT::SFTNullRound::RoundPhase::ViewChangeVotingPhase)
    {
        qDebug() << "Received voting" << this->localId << "";

        //It's okay if we're ahead, we still want to accomodate
        if (this->roundPhase >= SFT::SFTNullRound::RoundPhase::ViewChangeVotingPhase)
        {
            qDebug() << "Responding to voting" << this->localId << "";


            //Basically check if our proposed view is in agreement
            QVariantMap resp = QVariantMap();
            int viewNum = map.value("ViewNum").toInt();

            resp.insert("Type", QVariant(MessageTypes::ViewChangeVote));
            resp.insert("ViewNum", QVariant(viewNum));
            resp.insert("Vote", QVariant(this->sftViewManager->getProposedViewNum() == viewNum));

            emit this->sendToSingleNode(from, resp);
            return;
        }
    }
    else if (type == SFT::SFTNullRound::RoundPhase::ClientAttendancePhase)
    {
        qDebug() << "Received attendance request" << this->localId << "";

        if (this->roundPhase >= SFT::SFTNullRound::RoundPhase::ClientAttendancePhase)
        {
            qDebug() << "Responding to attendance request" << this->localId << "";

            QVariantMap resp = QVariantMap();
            resp.insert("Type", QVariant(MessageTypes::ClientAttendance));
            resp.insert("ClientList", QVariant(getReceivedClients()));

            emit this->sendToSingleNode(from, resp);
            return;
        }
    }
    else
    {
        qDebug() << "This is wrong!!" << map;
    }
}


void SFTMessageManager::processViewChangeProposal(QVariantMap map, const Connections::Id &from)
{
    qDebug() << "Local Id:" << this->localId;
    //TODO: What about the last person that gets here?
    if (this->roundPhase != SFT::SFTNullRound::RoundPhase::ViewChangeVotingPhase)
    {
        qDebug() << "Wrong time for view change voting" << this->roundPhase;
        return;
    }

    if (this->respondedServers->contains(from.GetInteger().GetInt32()))
    {
        qDebug() << "Already received a message from this server";
        return;
    }

    //TODO: Should I be doing this at all?
    //this->roundPhase = SFT::SFTNullRound::RoundPhase::ViewChangeVotingPhase;

    //Also make sure not to double count per server!!! (how do I do that?)
    int viewNum = map.value("ViewNum").toInt();
    int vote = map.value("Vote").toBool();

    //Means the view changed
    int viewChangeCode = this->sftViewManager->addViewChangeVote(viewNum, vote, from);
    if (viewChangeCode > -1)
    {
        //Should notify clients somehow
        QVariantMap map = this->genClientViewChangeNotification(viewChangeCode);
        qDebug() << "View changed! " << this->sftViewManager->getCurrentViewNum();
        emit this->broadcastToDownstreamClients(map);

        QVariantMap map2 = this->genInputRequest();

        for (int i = 0; i < m_connections.count(); i++)
        {
            Connections::Id conn = m_connections.at(i);
            emit this->sendToSingleNode(conn, map2);
        }
        if (this->sftViewManager->inCurrentView(this->localId))
        {
            this->startRound();
        }
        //If it's not in the round then just keep it stalling until it's in the view
        else
        {
            this->roundPhase = SFT::SFTNullRound::RoundPhase::Collection;
        }
    }
    //Means that the view changed and also that the view change was rejected
    else if (viewChangeCode == -2)
    {
        //Start over
        this->startRound();
    }
}

void SFTMessageManager::processClientList(QVariantMap map, const Connections::Id &from)
{
    if (this->roundPhase != SFT::SFTNullRound::RoundPhase::ClientAttendancePhase)
    {
        qDebug() << "Wrong time for client attendance" << this->roundPhase;
        return;
    }

    if (!this->sftViewManager->inCurrentView(from))
    {
        qDebug() << "Sending server is not in current view, ignoring client list";
        return;
    }

    if (this->respondedServers->contains(from.GetInteger().GetInt32()))
    {
        qDebug() << "Already received a message from this server";
        return;
    }

    qDebug() << "Expecting " << this->sftViewManager->getViewSize() << " attendance responses";
    qDebug() << "Received " << this->respondedServers->count() + 1 << " attendance responses";

    //Add all of the round clients into the list
    QList<QVariant> clientList = map.value("ClientList").toList();
    for (int i = 0; i < clientList.count(); i++)
    {
        if (!this->totalRoundClients->contains(clientList.at(i).toInt()))
        {
            this->totalRoundClients->append(clientList.at(i).toInt());
        }
    }
    this->respondedServers->append(from.GetInteger().GetInt32());

    //TODO: This should really be ACTIVE servers
    if (this->respondedServers->count() == this->sftViewManager->getViewSize())
    {
        //Then we've got all of the clients
        startCipherExchange();
    }
}

//TODO: Cannot do duplicates!
void SFTMessageManager::processClientMessage(QVariantMap map, const Connections::Id &from)
{
    qDebug() << "Processing client message";
    if (this->roundPhase != SFT::SFTNullRound::RoundPhase::CollectionPhase)
    {
        qDebug() << "Cannot collect messages anymore";
        return;
    }
    else if (!this->m_connections.contains(from))
    {
        qDebug() << "Received message from wrong client";
        return;
    }
    else if (this->respondedClients->contains(from.GetInteger().GetInt32()))
    {
        qDebug() << "Client already submitted message";
        return;
    }

    qDebug() << "Received message from correct client";
    qDebug() << "Expecting " << this->m_connections.count() << " client messages";

    int viewNum = map.value("ViewNum").toInt();


    if (this->sftViewManager->getCurrentViewNum() != viewNum)
    {
        qDebug() << "Wrong view number!" << map;
        //Should we tell clients to change their views?
        return;
    }
    qDebug() << "Received " << this->respondedClients->count() + 1 << " client messages";


    QByteArray msg = map.value("Message").toByteArray();
    int clientID = from.GetInteger().GetInt32();
    receivedClientMsgs->insert(clientID, msg);

    if (!this->respondedClients->contains(clientID))
    {
        this->respondedClients->append(clientID);
    }

    if (this->respondedClients->count() == this->m_connections.count())
    {
        //Switch into next phase
        startClientAttendance();
    }

}

//For the client
void SFTMessageManager::processViewChangeNotification(QVariantMap map, const Connections::Id &from)
{

    //TODO: This should be servers in the round!
    if (!this->m_servers.Contains(from))
    {
        qDebug() << "View change notification invalid. Not from a server in the round";
        return;
    }

    int viewNum = map.value("ViewNum").toInt();

    qDebug() << "Attempted view change from " << from << "to " << this->localId;

    if (viewNum == this->sftViewManager->getCurrentViewNum())
    {
        qDebug() << "View stays the same";
        return;
    }

    qDebug() << "View was changed" << map;


    this->sftViewManager->setNewView(viewNum);

    //TODO: Take this out later!
    this->clientRoundMessageSend("def");

}

//For the client
void SFTMessageManager::processDecrypted(QVariantMap map)
{
    QByteArray b = map.value("Decrypted").toByteArray();
    QString finalOutput = QString(b).replace('\0', ' '); //Need this for displaying
    qDebug() << "Decrypted message" << map << b << finalOutput;
}




}
}
