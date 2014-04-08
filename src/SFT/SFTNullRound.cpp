#include "SFTNullRound.hpp"

namespace Dissent {
namespace SFT {
  SFTNullRound::SFTNullRound(const Identity::Roster &clients,
      const Identity::Roster &servers,
      const Identity::PrivateIdentity &ident,
      const QByteArray &nonce,
      const QSharedPointer<ClientServer::Overlay> &overlay,
      Messaging::GetDataCallback &get_data) :
    Round(clients, servers, ident, nonce, overlay, get_data),
    m_received(servers.Count() + clients.Count()),
    m_msgs(0)
  {
      this->isServer = false;
      if (servers.Contains(ident.GetId())) {this->isServer = true; qDebug() << "SFT is a server";}
      else if (clients.Contains(ident.GetId())) {qDebug() <<  "SFT is a client";}
      else {qDebug() << "SFT is neither";}

      this->sftViewManager = new SFTViewManager(servers);
      this->sftMessageManager = new SFTMessageManager(this->GetLocalId(), clients, servers, sftViewManager);

      qDebug() << "Signals connected";
      connect(this->sftMessageManager, SIGNAL(broadcastToClients(QVariantMap)), this, SLOT(broadcastToClients(QVariantMap)));
      connect(this->sftMessageManager, SIGNAL(broadcastToServers(QVariantMap)), this, SLOT(broadcastToServers(QVariantMap)));
      connect(this->sftMessageManager, SIGNAL(sendToSingleServer(Connections::Id,QVariantMap)), this, SLOT(sendToSingleServer(Connections::Id,QVariantMap)));

      qDebug() << "SFTNullRound constructor IS WORKING " << ident.GetKey();
  }

  void SFTNullRound::HandleDisconnect(const Connections::Id &id)
  {
      //Also modifies the list
      if (this->sftViewManager->addFailedServer(id))
      {
        int newView = this->sftViewManager->nextGoodView(this->sftViewManager->getCurrentViewNum() + 1);
        this->sftViewManager->addViewChangeVote(newView, this->GetLocalId()); //Remember to vote for myself
        //ADD BACK IN    this->sftMessageManager->sendViewChangeProposal(newView);
      }

      if (this->sftViewManager->tooFewServers())
      {
          SetSuccessful(false);
          Stop("Round killed by too many server disconnects");
      }
  }

  void SFTNullRound::OnStart()
  {
    Round::OnStart();

    if (isServer) {ServerOnStart();}
    else {ClientOnStart();}

    //Not really sure what all of this does
    QPair<QByteArray, bool> data = GetData(1024);
    QByteArray msg = data.first;
    msg.prepend(127);
  }

  //TODO: Is there even a way to broadcast specifically to clients?
  void SFTNullRound::broadcastToClients(QVariantMap map)
  {
      QVariant variant(map);
      QByteArray data;
      QDataStream stream(&data,QIODevice::WriteOnly);
      stream << variant;
      QByteArray msg;
      msg.append(data);
      msg.prepend(127);
      GetOverlay()->Broadcast("SessionData", msg); //TODO: Why can't I broadcast to clients?
  }

  void SFTNullRound::broadcastToServers(QVariantMap map)
  {
      QVariant variant(map);
      QByteArray data;
      QDataStream stream(&data,QIODevice::WriteOnly);
      stream << variant;
      QByteArray msg;
      msg.append(data);
      msg.prepend(127);
      GetOverlay()->BroadcastToServers("SessionData", msg);
  }

  void SFTNullRound::sendToSingleServer(const Connections::Id &to, QVariantMap map)
  {
      QVariant variant(map);
      QByteArray data;
      QDataStream stream(&data,QIODevice::WriteOnly);
      stream << variant;
      QByteArray msg;
      msg.append(data);
      msg.prepend(127);
      GetOverlay()->SendNotification(to, "SessionData", msg);
  }

  void SFTNullRound::ClientOnStart()
  {
      QVariantMap map;
      map.insert("Type", SFTMessageManager::MessageTypes::ClientMessage); //TODO: Just a test filler
      broadcastToServers(map);
  }

  void SFTNullRound::ServerOnStart()
  {
      QVariantMap map;
      map.insert("Type", SFTMessageManager::MessageTypes::ClientAttendance); //TODO: Just a test filler
      broadcastToServers(map);
  }

  void SFTNullRound::ProcessPacket(const Connections::Id &from, const QByteArray &data)
  {
    if(Stopped()) {
      qWarning() << "Received a message on a closed session:" << ToString();
      return;
    }

    if(!GetServers().Contains(from) && !GetClients().Contains(from)) {
      qDebug() << ToString() << " received wayward message from: " << from;
      return;
    }

    if (isServer)
    {
        ServerProcessPacket(from, data);
    }
    else
    {
        ClientProcessPacket(from, data);
    }

  }

  void SFTNullRound::ClientProcessPacket(const Connections::Id &from, const QByteArray &data)
  {
      //GetOverlay()->SendNotification(from, "TEST", data);

      qDebug() << "THIS IS BRANDON'S";

      QByteArray data2(data); //So that we can have a pointer to it
      QDataStream stream(&data2,QIODevice::ReadOnly);
      QVariant variant;
      stream>>variant;
      qDebug()<< qvariant_cast<QVariantMap>(variant);
      qDebug() << from;
      QVariantMap map = qvariant_cast<QVariantMap>(variant);
      this->sftMessageManager->switchClientMessage(map, from);



  }

  void SFTNullRound::ServerProcessPacket(const Connections::Id &from, const QByteArray &data)
  {
      QByteArray data2(data); //So that we can have a pointer to it
      QDataStream stream(&data2,QIODevice::ReadOnly);
      QVariant variant;
      stream>>variant;
      qDebug()<< qvariant_cast<QVariantMap>(variant);
      qDebug() << from;
      QVariantMap map = qvariant_cast<QVariantMap>(variant);
      this->sftMessageManager->switchServerMessage(map, from);
      //GetOverlay()->SendNotification(from, "TEST", data);
  }
}
}


// 1) Instead of checking for duplicates, I should look at what type of message it is and do the appropriate action
// 2) Keep track of who send messages but terminate round when we receive from everyone in the view, not entire server list
// 3) Everything else stays the same

/*
//MOVE THIS
int idx = 0;
if(GetOverlay()->IsServer(from)) {
  idx = GetServers().GetIndex(from);
} else {
  idx = GetServers().Count() + GetClients().GetIndex(from);
}
if(!m_received[idx].isEmpty()) {
  qWarning() << "Receiving a second message from: " << from;
  return;
}

if(!data.isEmpty()) {
  qDebug() << GetLocalId().ToString() << "received a real message from" << from;
}

m_received[idx] = data;
m_msgs++;

qDebug() << GetLocalId().ToString() << "received" <<
  m_msgs << "expecting" << m_received.size();

if(m_msgs != m_received.size()) {
  return;
}

foreach(const QByteArray &msg, m_received) {
  if(!msg.isEmpty()) {
    PushData(GetSharedPointer(), msg);
  }
}

SetSuccessful(true);
Stop("Round successfully finished.");*/

/*
QByteArray payload;
if(!Verify(from, data, payload)) {
  throw QRunTimeError("Invalid signature or data");
}

if(_state == Offline) {
  throw QRunTimeError("Should never receive a message in the bulk"
      " round while offline.");
}

QDataStream stream(payload);

int mtype;
QByteArray round_id;
stream >> mtype >> round_id;

MessageType msg_type = (MessageType) mtype;

Id rid(round_id);
if(rid != GetRoundId()) {
  throw QRunTimeError("Not this round: " + rid.ToString() + " " +
      GetRoundId().ToString());
}

if(_state == Shuffling) {
  _log.Pop();
  _offline_log.Append(data, from);
  return;
}

switch(msg_type) {
  case BulkData:
    HandleBulkData(stream, from);
    break;
  case LoggedBulkData:
    HandleLoggedBulkData(stream, from);
    break;
  case AggregatedBulkData:
    HandleAggregatedBulkData(stream, from);
    break;
  default:
    throw QRunTimeError("Unknown message type");
}*/
