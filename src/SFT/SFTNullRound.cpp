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

      //Why doesn't it like when I use new?
      this->downstreamClients = QList<Connections::Id>();
      this->upstreamServers = QList<Connections::Id>();
      this->sftViewManager = new SFTViewManager(servers);

      Connections::ConnectionTable connTable = this->GetOverlay()->GetConnectionTable();

      if (servers.Contains(ident.GetId()))
      {
          this->isServer = true;
          qDebug() << "SFT is a server";
          for (int i = 0; i < clients.Count(); i++)
          {
              Connections::Id client = clients.GetId(i);
              if (connTable.GetConnection(client) != 0)
              {
                  this->downstreamClients.append(client);
              }
          }
          qDebug() << "Client IDs: " << this->GetLocalId() << downstreamClients;
          this->sftMessageManager = new SFTMessageManager(this->GetLocalId(), clients, servers, downstreamClients, isServer, sftViewManager);

      }
      else if (clients.Contains(ident.GetId()))
      {
          qDebug() << "SFT is a client";
          for (int i = 0; i < servers.Count(); i++)
          {
              Connections::Id server = servers.GetId(i);
              if (connTable.GetConnection(server) != 0)
              {
                  this->upstreamServers.append(server);
              }
          }
          qDebug() << "Server ID: " << this->GetLocalId() << upstreamServers;
          this->sftMessageManager = new SFTMessageManager(this->GetLocalId(), clients, servers, upstreamServers, isServer, sftViewManager);

      }
      else
      {
          qDebug() << "SFT is neither";
      }


      qDebug() << "Signals connected";
      connect(this->sftMessageManager, SIGNAL(broadcastToDownstreamClients(QVariantMap)), this, SLOT(broadcastToDownstreamClients(QVariantMap)));
      connect(this->sftMessageManager, SIGNAL(broadcastToServers(QVariantMap)), this, SLOT(broadcastToServers(QVariantMap)));
      connect(this->sftMessageManager, SIGNAL(sendToSingleNode(Connections::Id,QVariantMap)), this, SLOT(sendToSingleNode(Connections::Id,QVariantMap)));

      qDebug() << "SFTNullRound constructor IS WORKING " << ident.GetKey();
  }

  void SFTNullRound::HandleDisconnect(const Connections::Id &id)
  {
      if(GetOverlay()->IsServer(id)) {
        qDebug() << "A server (" << id << ") disconnected, ignoring.";
        return;
      } else if(GetClients().Contains(id)) {
        qDebug() << "A client (" << id << ") disconnected, ignoring.";
        return;
      }

      qDebug() << "Server " << id << "failed!" << "watching from " << this->GetLocalId();
      //Also modifies the list
      if (this->sftViewManager->addFailedServer(id))
      {
          this->sftMessageManager->startViewVoting();
          //ADD BACK IN    this->sftMessageManager->sendViewChangeProposal(newView);
      }

      if (this->sftViewManager->tooFewServers())
      {
          SetSuccessful(false);
          qDebug() << "Round killed by too many server disconnects";
          Stop("Round killed by too many server disconnects");
      }
  }

  void SFTNullRound::OnStop()
  {
      //Do nothing
      qDebug() << "Should be hitting this" << Stopped();
  }

  void SFTNullRound::OnStart()
  {
    Round::OnStart();

    qDebug() << "CHECKING " << this->upstreamServers.count() << this->downstreamClients.count();

    if (isServer) {ServerOnStart();}
    else {ClientOnStart();}

    //Not really sure what all of this does
    QPair<QByteArray, bool> data = GetData(1024);
    QByteArray msg = data.first;
    msg.prepend(127);
  }

  //TODO: Is there even a way to broadcast specifically to clients?
  void SFTNullRound::broadcastToDownstreamClients(QVariantMap map)
  {
      QVariant variant(map);
      QByteArray data;
      QDataStream stream(&data,QIODevice::WriteOnly);
      stream << variant;
      QByteArray msg;
      msg.append(data);
      msg.prepend(127);

      for (int i = 0; i < this->downstreamClients.count(); i++)
      {
          Connections::Id client = this->downstreamClients.at(i);
          GetOverlay()->SendNotification(client, "SessionData", msg);
      }
      //GetOverlay()->Broadcast("SessionData", msg); //TODO: Why can't I broadcast to clients?
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

  void SFTNullRound::sendToSingleNode(const Connections::Id &to, QVariantMap map)
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
      this->sftMessageManager->clientRoundMessageSend("abc");
  }

  void SFTNullRound::ServerOnStart()
  {
      this->sftMessageManager->startViewVoting();
      /*QVariantMap map;
      map.insert("Type", SFTMessageManager::MessageTypes::ClientAttendance); //TODO: Just a test filler
      broadcastToServers(map);*/
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
