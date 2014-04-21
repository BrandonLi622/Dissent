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
      this->roundNumber = 0;

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
      connect(this->sftMessageManager, SIGNAL(broadcastToEveryone(QVariantMap)), this, SLOT(broadcastToEveryone(QVariantMap)));

      connect(this->sftMessageManager, SIGNAL(sendToSingleNode(Connections::Id,QVariantMap)), this, SLOT(sendToSingleNode(Connections::Id,QVariantMap)));
      connect(this->sftMessageManager, SIGNAL(pushDataOut(QByteArray)), this, SLOT(pushDataOut(QByteArray)));
      connect(this->sftMessageManager, SIGNAL(messageExchangeEnded()), this, SLOT(startMessageExchange()));


      //connect(this->sftMessageManager, SIGNAL(roundSuccessful()), this, SLOT(endSuccessful()));
      qDebug() << "SFTNullRound constructor IS WORKING " << ident.GetKey();
  }

  void SFTNullRound::pushDataOut(QByteArray data)
  {
      qDebug() << "Push data getting called";
      /*QByteArray data2(data); //So that we can have a pointer to it
      QDataStream stream(&data2,QIODevice::ReadOnly);
      QVariant variant;
      stream>>variant;
      final = qvariant_cast<QString>(variant);
      qDebug() << final;*/

      this->PushData(this->GetSharedPointer(), data);
      //this->GetData(1024); // we want it to flush

      //Probably don't want to start this immediately...


      if (this->GetClients().Contains(this->GetLocalId()))
      {
          //QTimer::singleShot(500, this, SLOT(startMessageExchange()));
          startMessageExchange();
      }

      this->roundNumber++;


      //endSuccessful();
  }

  void SFTNullRound::HandleDisconnect(const Connections::Id &id)
  {
      if(GetOverlay()->IsServer(id)) {
        qDebug() << "A server (" << id << ") disconnected, ignoring.";
      } else if(GetClients().Contains(id)) {
        qDebug() << "A client (" << id << ") disconnected, ignoring.";
        return;
      }

      qDebug() << "Round" << this->roundNumber << ": Server" << id << "failed! watching from " << this->GetLocalId() << this->isServer;

      //Also modifies the list
      //TODO: This is incorrect, model based on below
      if (this->sftViewManager->addFailedServer(id))
      {
          if (this->sftViewManager->tooFewServers())
          {
              if (this->Stopped())
              {
                  return;
              }

              int numLiveServers = this->sftViewManager->getNumLiveServers();
              SetSuccessful(false);
              qDebug() << "Round killed by too many server disconnects" << numLiveServers << this->GetLocalId();
              Stop("2Round killed by too many server disconnects" + QString::number(numLiveServers));
              return;
          }

          qDebug() << "Starting view voting";
          this->sftMessageManager->startViewVoting();
          //ADD BACK IN    this->sftMessageManager->sendViewChangeProposal(newView);
      }
  }

  void SFTNullRound::OnStop()
  {
      //Do nothing
      qDebug() << "Should be hitting this" << Stopped();
  }

  void SFTNullRound::OnStart()
  {
      qDebug() << "Printing connection table";
      this->GetOverlay()->GetConnectionTable().PrintConnectionTable();

    Round::OnStart();

    qDebug() << "CHECKING " << this->upstreamServers.count() << this->downstreamClients.count();

    if (isServer) {ServerOnStart();}
    else {ClientOnStart();}
  }

  //TODO: Probably want a way to broadcast view change to ALL clients


  //TODO: Is there even a way to broadcast specifically to clients?
  void SFTNullRound::broadcastToDownstreamClients(QVariantMap map)
  {
      QVariant variant(map);
      QByteArray data;
      QDataStream stream(&data,QIODevice::WriteOnly);
      stream << variant;

      QByteArray msg = GetHeaderBytes() + data;

      for (int i = 0; i < this->downstreamClients.count(); i++)
      {
          Connections::Id client = this->downstreamClients.at(i);
          GetOverlay()->SendNotification(client, "SessionData", msg);
      }
      //GetOverlay()->Broadcast("SessionData", msg); //TODO: Why can't I broadcast to clients?
  }

  void SFTNullRound::broadcastToEveryone(QVariantMap map)
  {
      QVariant variant(map);
      QByteArray data;
      QDataStream stream(&data,QIODevice::WriteOnly);
      stream << variant;
      QByteArray msg = GetHeaderBytes() + data;

      GetOverlay()->Broadcast("SessionData", msg);
  }

  void SFTNullRound::broadcastToServers(QVariantMap map)
  {
      QVariant variant(map);
      QByteArray data;
      QDataStream stream(&data,QIODevice::WriteOnly);
      stream << variant;
      QByteArray msg = GetHeaderBytes() + data;

      GetOverlay()->BroadcastToServers("SessionData", msg);
  }

  void SFTNullRound::sendToSingleNode(const Connections::Id &to, QVariantMap map)
  {
      QVariant variant(map);
      QByteArray data;
      QDataStream stream(&data,QIODevice::WriteOnly);
      stream << variant;
      QByteArray msg = GetHeaderBytes() + data;

      GetOverlay()->SendNotification(to, "SessionData", msg);
  }


  //For the clients
  void SFTNullRound::startMessageExchange()
  {
      QString final = "";

      qDebug() << "Starting a message exchange!" << this->GetLocalId();

      //while (final == "") {
          QPair<QByteArray, bool> data = GetData(1024);
          QByteArray msg = data.first;

          QByteArray data2(msg); //So that we can have a pointer to it
          QDataStream stream(&data2,QIODevice::ReadOnly);
          QVariant variant;
          stream>>variant;
          final = qvariant_cast<QString>(variant);
          qDebug() << "Got Data at " << this->GetLocalId() << final;
      //}


      //TODO: JUST A HACK
      QString final2("abc");

      //Send the data that is to be sent in this round
      this->sftMessageManager->clientRoundMessageSend(final2);
  }

  void SFTNullRound::ClientOnStart()
  {
      startMessageExchange();
  }

  void SFTNullRound::ServerOnStart()
  {
      //checkOnlineServers();
  }

  /*
  void SFTNullRound::checkOnlineServers()
  {
      int newView;

      for (int i = 0; i < this->GetServers().Count(); i++)
      {
          Connections::Id server = GetServers().GetId(i);

          if (this->GetOverlay()->GetConnectionTable().GetConnection(server) == 0)
          {
              qDebug() << "SFT Server is disconnected: " << server;
              newView = this->sftViewManager->addFailedServer(server);
          }
      }

      //If there aren't enough servers, then we cannot perform the round
      if (this->sftViewManager->tooFewServers())
      {
          SetSuccessful(false);
          qDebug() << "Round killed by too many server disconnects";
          Stop("Round killed by too many server disconnects");
      }

      if (newView != this->sftViewManager->getCurrentViewNum())
      {
          qDebug() << "We are starting the view voting";
          this->sftMessageManager->startViewVoting();
      }
  }*/

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
  }

  void SFTNullRound::endSuccessful()
  {
      SetSuccessful(true);
      Stop("Round successfully finished.");
      qDebug() << "Round done" << this->GetLocalId();
  }
}
}


