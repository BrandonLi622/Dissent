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


    //this->respondedForwardedClients = new QList<int>();


    this->respondedServers_Attendance = new QList<int>();
    this->respondedServers_Cipher = new QList<int>();
    this->respondedServers_ViewChange = new QList<int>();

    this->totalRoundClients = new QList<int>();

    this->timer = new QTimer();
    this->timer->setSingleShot(true);
    this->connect(this->timer, SIGNAL(timeout()), this, SLOT(startClientAttendance()));

    if (isServer)
    {
        this->startServerRound();
    }
    else
    {
        this->roundPhase = SFTNullRound::ClientRoundPhase::ReceivePhase;
    }
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
void SFTMessageManager::startServerRound()
{

    qDebug() << "\n\n\nStarting Round" << this->localId << "\n\n\n";
    qDebug() << "Initial View" << this->sftViewManager->getCurrentViewNum() << this->sftViewManager->getCurrentServers();

    this->roundPhase = SFT::SFTNullRound::ServerRoundPhase::CollectionPhase;
    this->receivedClientMsgs->clear();
    this->receivedServerCiphers->clear();
    this->respondedClients->clear();
    this->respondedServers_Attendance->clear();
    this->respondedServers_Cipher->clear();
    this->totalRoundClients->clear();


    //this->respondedServers_ViewChange->clear();
}

void SFTMessageManager::startClientAttendance()
{
    this->timer->stop();

    qDebug() << "\n\n\nStarting Client Attendance" << this->localId << "\n\n\n";
    qDebug() << "Number of responding clients: " << this->respondedClients->count();

    this->respondedServers_Attendance->clear();
    this->respondedServers_Attendance->append(this->localId.GetInteger().GetInt32());
    for (int i = 0; i < this->respondedClients->count(); i++)
    {
        int id = this->respondedClients->at(i);
        this->totalRoundClients->append(id);
    }
    qDebug() << "Total round clients are: " << *totalRoundClients;
    QList<QVariant> serializedClientList = getReceivedClients();
    this->roundPhase = SFT::SFTNullRound::ServerRoundPhase::ClientAttendancePhase;
    QVariantMap map = QVariantMap();
    map.insert("Type", MessageTypes::ClientAttendance);
    map.insert("ClientList", QVariant(serializedClientList));

    emit this->broadcastToServers(map);
    qDebug() << "Sending ClientAttendance request" << this->localId;
    sendRequest(SFT::SFTNullRound::ServerRoundPhase::ClientAttendancePhase);

    //Need to ask for attendance lists from other servers

}

void SFTMessageManager::startCipherExchange()
{
    qDebug() << "\n\n\nStarting Cipher Exchange" << this->localId << "\n\n\n";

    this->receivedServerCiphers->clear();
    this->respondedServers_Cipher->clear();
    this->respondedServers_Cipher->append(this->localId.GetInteger().GetInt32());
    this->roundPhase = SFT::SFTNullRound::ServerRoundPhase::ExchangeCiphersPhase;
    QByteArray cipherText = this->createCipher();
    QVariantMap map = QVariantMap();
    map.insert("Type", MessageTypes::ServerCipher);
    map.insert("Cipher", cipherText);
    this->receivedServerCiphers->insert(this->localId.GetInteger().GetInt32(), cipherText);
    qDebug() << this->localId << "Own cipher responses: " << cipherText;
    this->lastCipher = cipherText; //Save it for use when prompted

    qDebug() << "My cipher: " << map;

    emit this->broadcastToServers(map);
    sendRequest(SFT::SFTNullRound::ServerRoundPhase::ExchangeCiphersPhase);
}

//Not sure if we should just not receive any votes until ready...
void SFTMessageManager::startViewVoting()
{
    qDebug() << "\n\n\nStarting View Voting" << this->localId << "\n\n\n";

    this->roundPhase = SFT::SFTNullRound::ServerRoundPhase::ViewChangeVotingPhase;


    int newView = this->sftViewManager->nextGoodView(this->sftViewManager->getCurrentViewNum());

    qDebug() << "View voting done";


    this->sftViewManager->proposeNewView(newView);
    this->sftViewManager->addViewChangeVote(newView, true, this->localId); //Remember to vote for myself

    qDebug() << "Proposing a new view: " << newView;

    sendViewChangeProposal(newView);
    sendRequest(SFT::SFTNullRound::ServerRoundPhase::ViewChangeVotingPhase);

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

//Just send it to all of the servers, I suppose
/*
void SFTMessageManager::forwardClientMessage(QVariantMap msg, const Connections::Id &from)
{
    msg.insert("Type", SFTMessageManager::MessageTypes::ForwardedClientMessage);
    msg.insert("ClientId", from.GetInteger().GetInt32());

    //Send to the first online server
    Connections::Id peer = this->sftViewManager->getCurrentServers().at(0);
    qDebug() << "Forwarding to: " << peer << msg;
    emit this->sendToSingleNode(peer, msg);
    //emit this->broadcastToServers(msg);
}*/

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
        for (int j = 0; j < SFT::SFTMessageManager::keyLength; j++)
        {
            fullMsg.append(char(0));
        }
    }

    fullMsg.append(msg); //Add in my message;

    for (int i = index + 1; i < this->m_clients.Count(); i++)
    {
        //null values for other people's
        for (int j = 0; j < SFT::SFTMessageManager::keyLength; j++)
        {
            fullMsg.append(char(0));
        }
    }

    QByteArray testMsg;
    testMsg.append(fullMsg);
    qDebug() << "Client: " << this->m_clients.Contains(this->localId) << this->m_clients.Count();
    qDebug() << "Server: " << this->m_servers.Contains(this->localId) << this->m_servers.Count();
    qDebug() << this->localId;
    qDebug() << "Indeces: " << index << this->m_clients.Count();
    qDebug() << "Full msg: " << fullMsg.length() << testMsg.replace('\0', ' ');

    return fullMsg;
}

QByteArray SFTMessageManager::encryptClientMessage(int clientID, QByteArray data)
{
    QByteArray xored(data);
    QVector<Connections::Id> servers = this->sftViewManager->getCurrentServers();
    //Should only encrypt based on who is actually in the round!!

    qDebug() << "Client knows: " << servers.count() << "servers";
    for (int i = 0; i < servers.count(); i++)
    {
        qDebug() << "Before xor: " << xored.length() << xored;

        int serverID = servers.at(i).GetInteger().GetInt32();
        //This should really be the number of clients * keylength
        QByteArray key = encryptionKey(serverID, clientID, SFT::SFTMessageManager::keyLength, this->m_clients.Count());
        qDebug() << "Xoring bytes for client message encryption";
        xored = xorBytes(xored, key);

        qDebug() << "After xor: " << xored;
    }
    qDebug() << "Client's message length: " << xored.length();

    return xored;
}

void SFTMessageManager::clientRoundMessageSend(QString msg)
{
    QString sub = msg.mid(0, SFT::SFTMessageManager::keyLength); //Truncate the message to what we can actually send
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
    qDebug() << receivedClientMsgs->keys().length() << "received keys" << *receivedClientMsgs;

    //Xor's the bits from the client first
    int id = receivedClientMsgs->keys().value(0);
    QByteArray xored = receivedClientMsgs->value(id);
    for (int i = 1; i < receivedClientMsgs->keys().length(); i++)
    {
        id = receivedClientMsgs->keys().value(i);
        xored = xorBytes(xored, receivedClientMsgs->value(id));
    }

    int thisID = this->localId.GetInteger().GetInt32();

    //If we're not in the view, then no need to get keys
    if (!this->sftViewManager->inCurrentView(this->localId))
    {
        return xored;
    }

    //Assuming fixed length messages
    //Gets the encryption key for all of the clients
    qDebug() << totalRoundClients->count() << " total round clients" << *totalRoundClients;
    for (int i = 0; i < totalRoundClients->count(); i++)
    {
        int clientID = totalRoundClients->at(i);
        QByteArray key = encryptionKey(thisID, clientID, SFT::SFTMessageManager::keyLength, int(this->m_clients.Count()));

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
    /*
    else if ((MessageTypes) map.value("Type").toInt() == SFT::SFTMessageManager::MessageTypes::ForwardedClientMessage)
    {
        qDebug() << "Received a forwarded client message";
        processForwardedClientMessage(map, from);
    }*/
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
    if (this->roundPhase != SFT::SFTNullRound::ServerRoundPhase::ExchangeCiphersPhase)
    {
        qDebug() << "Wrong time for cipher texts" << this->roundPhase;
        return;
    }

    /*
    if (!this->sftViewManager->inCurrentView(from))
    {
        qDebug() << "Sending server is not in current view, ignoring ciphertext";
        return;
    }*/

    if (this->respondedServers_Cipher->contains(from.GetInteger().GetInt32()))
    {
        qDebug() << "Already received a message from this server";
        return;
    }

    QByteArray cipher = map.value("Cipher").toByteArray();


    qDebug() << this->localId << "Expecting " << this->sftViewManager->getNumLiveServers() << " cipher responses";
    qDebug() << this->localId << "Received " << this->receivedServerCiphers->count() + 1 << " cipher responses" << this->localId << cipher;
    int serverID = from.GetInteger().GetInt32();

    receivedServerCiphers->insert(serverID, cipher);

    if (!respondedServers_Cipher->contains(serverID))
    {
        respondedServers_Cipher->append(serverID);
    }
    qDebug() << "Received cipher";

    if (receivedServerCiphers->count() == this->sftViewManager->getNumLiveServers())
    {
        //DONE WITH THE ROUND!
        //Should now decrypt and send back to clients
        QVariantMap map = genDecryption(this->sftViewManager->getCurrentViewNum());
        QByteArray decrypted = map.value("Decrypted").toByteArray();
        qDebug() << "Done with decryption" << this->sftViewManager->getNumLiveServers() << this->sftViewManager->getCurrentViewNum() << this->localId << decrypted;

        //TODO: Should really only be sending to own clients...
        emit this->broadcastToDownstreamClients(map);
        emit this->roundSuccessful();
        this->startServerRound();  //TODO: This is going to be a problem for synchronization...

        emit this->pushDataOut(decrypted);
    }
}

void SFTMessageManager::processInputRequest(QVariantMap map, const Connections::Id &from)
{
    SFT::SFTNullRound::ServerRoundPhase type = (SFT::SFTNullRound::ServerRoundPhase) map.value("Request").toInt();
    if (type == SFT::SFTNullRound::ServerRoundPhase::CollectionPhase)
    {
        qDebug() << "Received input request" << this->localId << "from " << from;

        //Client doesn't really have phases they care about
        //this->clientRoundMessageSend("def"); //TODO: Remove this
        return;
    }
}

void SFTMessageManager::processInfoRequest(QVariantMap map, const Connections::Id &from)
{
    SFT::SFTNullRound::ServerRoundPhase type = (SFT::SFTNullRound::ServerRoundPhase) map.value("Request").toInt();

    //Not-in-view servers cannot participate in this
    if (type == SFT::SFTNullRound::ServerRoundPhase::ExchangeCiphersPhase)
    {
        /*
        if (!this->sftViewManager->inCurrentView(this->localId))
        {
            qDebug() << "This local server is not in the current view" << this->localId;
            return;
        }*/

        qDebug() << "Received ciphers request" << this->localId << "";

        if (this->roundPhase >= SFT::SFTNullRound::ServerRoundPhase::ExchangeCiphersPhase ||
            this->roundPhase == SFT::SFTNullRound::ServerRoundPhase::CollectionPhase)
        {
            qDebug() << "Responding to ciphers request" << this->localId << "";

            QVariantMap resp = QVariantMap();
            resp.insert("Type", QVariant(MessageTypes::ServerCipher));
            resp.insert("Cipher", QVariant(this->lastCipher));

            emit this->sendToSingleNode(from, resp);
            return;
        }
    }
    else if (type == SFT::SFTNullRound::ServerRoundPhase::ViewChangeVotingPhase)
    {
        qDebug() << "Received voting" << this->localId << "";

        //It's okay if we're ahead, we still want to accomodate
        if (this->roundPhase >= SFT::SFTNullRound::ServerRoundPhase::ViewChangeVotingPhase)
        {
            qDebug() << "Responding to voting" << this->localId << "";


            //Basically check if our proposed view is in agreement
            QVariantMap resp = QVariantMap();
            int viewNum = map.value("ViewNum").toInt();

            qDebug() << resp << viewNum;

            //TODO: Does this really do anything? Or should we just send our current view?
            /*resp.insert("Type", QVariant(MessageTypes::ViewChangeVote));
            resp.insert("ViewNum", QVariant(viewNum));
            resp.insert("Vote", QVariant(this->sftViewManager->getProposedViewNum() == viewNum));

            emit this->sendToSingleNode(from, resp);*/
            return;
        }

        else
        {
            qDebug() << "Not yet in voting stage" << this->localId;
        }
    }
    //Not-in-view servers cannot participate in this, for now
    else if (type == SFT::SFTNullRound::ServerRoundPhase::ClientAttendancePhase)
    {
        /*
        if (!this->sftViewManager->inCurrentView(this->localId))
        {
            qDebug() << "This local server is not in the current view" << this->localId;
            return;
        }*/

        qDebug() << "Received attendance request" << this->localId << "";

        if (this->roundPhase == SFT::SFTNullRound::ServerRoundPhase::ClientAttendancePhase ||
            this->roundPhase == SFT::SFTNullRound::ServerRoundPhase::ExchangeCiphersPhase)
        {
            qDebug() << "Responding to attendance request" << this->localId << "";

            QVariantMap resp = QVariantMap();
            resp.insert("Type", QVariant(MessageTypes::ClientAttendance));
            resp.insert("ClientList", QVariant(getReceivedClients()));

            emit this->sendToSingleNode(from, resp);
            return;
        }

        else
        {
            qDebug() << "Not yet in attendance request stage" << this->localId;
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

    /*
    if (this->roundPhase != SFT::SFTNullRound::ServerRoundPhase::ViewChangeVotingPhase)
    {
        qDebug() << "Wrong time for view change voting" << this->roundPhase;
        return;
    }*/

    qDebug() << "View change vote message:" << this->localId << from << map;


    if (this->respondedServers_ViewChange->contains(from.GetInteger().GetInt32()))
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
        qDebug() << "View changed! " << this->sftViewManager->getCurrentViewNum() << this->sftViewManager->getCurrentServers();
        emit this->broadcastToDownstreamClients(map);


        /*
        QVariantMap map2 = this->genInputRequest();

        for (int i = 0; i < m_connections.count(); i++)
        {
            Connections::Id conn = m_connections.at(i);
            emit this->sendToSingleNode(conn, map2);
        }*/

        //if (this->sftViewManager->inCurrentView(this->localId))
        //{
            this->startServerRound();
        //}
        //If it's not in the round then just keep it stalling until it's in the view
        /*else
        {
            this->roundPhase = SFT::SFTNullRound::ServerRoundPhase::CollectionPhase;
        }*/

        this->respondedServers_ViewChange->clear();
    }
    //Means that we tried to change the view and also that the view change was rejected
    else if (viewChangeCode == -2)
    {
        //Start over
        this->startServerRound();
        this->respondedServers_ViewChange->clear();
    }
}

void SFTMessageManager::processClientList(QVariantMap map, const Connections::Id &from)
{
    /*
    if (!this->sftViewManager->inCurrentView(this->localId))
    {
        qDebug() << "This local server is not in the current view" << this->localId;
        return;
    }*/

    if (this->roundPhase != SFT::SFTNullRound::ServerRoundPhase::ClientAttendancePhase)
    {
        qDebug() << "Wrong time for client attendance" << this->roundPhase;
        return;
    }

    /*
    if (!this->sftViewManager->inCurrentView(from))
    {
        qDebug() << "Sending server is not in current view, ignoring client list";
        return;
    }*/

    if (this->respondedServers_Attendance->contains(from.GetInteger().GetInt32()))
    {
        qDebug() << "Already received a message from this server";
        return;
    }

    qDebug() << "Expecting " << this->sftViewManager->getNumLiveServers() << " attendance responses" << this->localId;
    qDebug() << "Received " << this->respondedServers_Attendance->count() + 1 << " attendance responses" << this->localId;

    //Add all of the round clients into the list
    QList<QVariant> clientList = map.value("ClientList").toList();
    for (int i = 0; i < clientList.count(); i++)
    {
        if (!this->totalRoundClients->contains(clientList.at(i).toInt()))
        {
            this->totalRoundClients->append(clientList.at(i).toInt());
        }
    }
    this->respondedServers_Attendance->append(from.GetInteger().GetInt32());

    //TODO: This should really be ACTIVE servers
    if (this->respondedServers_Attendance->count() == this->sftViewManager->getNumLiveServers())
    {
        //Then we've got all of the clients
        startCipherExchange();
    }
}

/*
void SFTMessageManager::processForwardedClientMessage(QVariantMap map, const Connections::Id &from)
{
    if (this->roundPhase != SFT::SFTNullRound::ServerRoundPhase::CollectionPhase)
    {
        qDebug() << "Cannot collect messages anymore";
        return;
    }

    //TODO: Should probably also check that the sender is a server
    if (this->m_servers.GetIndex(from) < 0)
    {
        qDebug() << "Sender of forwarded client message is not a server";
        return;
    }

    int clientId = map.value("ClientId").toInt();

    if (this->respondedForwardedClients->contains(clientId))
    {
        qDebug() << "Client already submitted message";
        return;
    }

    qDebug() << "Received message from correct client";

    int viewNum = map.value("ViewNum").toInt();


    if (this->sftViewManager->getCurrentViewNum() != viewNum)
    {
        qDebug() << "Wrong view number!" << map << this->sftViewManager->getCurrentViewNum();
        //TODO: Should we tell clients to change their views?
        return;
    }
    qDebug() << "Received " << this->respondedForwardedClients->count() + 1 << " client messages";

    if (!this->respondedForwardedClients->contains(clientId))
    {
        this->respondedForwardedClients->append(clientId);
    }

    QByteArray msg = map.value("Message").toByteArray();
    receivedClientMsgs->insert(clientId, msg);

    return;
}*/


//TODO: Cannot do duplicates!
void SFTMessageManager::processClientMessage(QVariantMap map, const Connections::Id &from)
{

    /*
    if (!this->sftViewManager->inCurrentView(this->localId))
    {
        qDebug() << "This local server is not in the current view" << this->localId;
        forwardClientMessage(map, from);
        return;
    }*/

    qDebug() << "Processing client message" << map << from;
    if (this->roundPhase != SFT::SFTNullRound::ServerRoundPhase::CollectionPhase)
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
        qDebug() << "Wrong view number!" << map << this->sftViewManager->getCurrentViewNum();
        //TODO: Should we tell clients to change their views?
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

    //TODO: Getting rid of this in favor of a timer
    //TODO: Not every client can get their message sent in. Too bad?
    if (this->respondedClients->count() == this->m_connections.count())
    {
        /*
        if (this->sftViewManager->getCurrentServers().indexOf(this->localId) == 0)
        {
            qDebug() << "This is the first server in the view" << this->localId;
            this->respondedClients->append(*(this->respondedForwardedClients));

            //timer->start(50);
            //return;
        }*/

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
    emit this->messageExchangeEnded();

    //TODO: Take this out later!
    //this->clientRoundMessageSend("def");

}

//For the client
void SFTMessageManager::processDecrypted(QVariantMap map)
{
    //TODO: Should probably check if we are in a receiving phase

    QByteArray b = map.value("Decrypted").toByteArray();
    QString finalOutput = QString(b).replace('\0', ' '); //Need this for displaying
    qDebug() << "Decrypted message" << map << b << finalOutput;

    //Call this in the NullRound
    this->roundPhase = SFTNullRound::ClientRoundPhase::SendPhase;
    emit this->pushDataOut(b);
}




}
}
