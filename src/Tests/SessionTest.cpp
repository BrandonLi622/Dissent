#include "DissentTest.hpp"
#include "OverlayTest.hpp"
#include "SessionTest.hpp"

#include "SFT/SFTMessageManager.hpp"

namespace Dissent {
namespace Tests {
  Sessions BuildSessions(const OverlayNetwork &network, CreateRound create_round)
  {
    DsaPrivateKey shared_key;
    QSharedPointer<KeyShare> keys(new KeyShare());

    Sessions sessions;
    sessions.network = network;
    sessions.keys = keys;
    sessions.create_round = create_round;

    foreach(const OverlayPointer &server, network.first) {
      QSharedPointer<AsymmetricKey> key(new DsaPrivateKey(
            shared_key.GetModulus(), shared_key.GetSubgroupOrder(),
            shared_key.GetGenerator()));
      keys->AddKey(server->GetId().ToString(), key->GetPublicKey());

      ServerPointer ss = MakeSession<ServerSession>(
            server, key, keys, create_round);
      sessions.servers.append(ss);
      sessions.private_keys[server->GetId().ToString()] = key;

      QSharedPointer<BufferSink> sink(new BufferSink());
      sessions.sinks.append(sink);

      QSharedPointer<SignalSink> ssink(new SignalSink());
      sessions.signal_sinks.append(ssink);

      QSharedPointer<SinkMultiplexer> sinkm(new SinkMultiplexer());
      sessions.sink_multiplexers.append(sinkm);
      sinkm->AddSink(sink);
      sinkm->AddSink(ssink);

      ss->SetSink(sinkm.data());
    }

    QList<ClientPointer> clients;
    foreach(const OverlayPointer &client, network.second) {
      QSharedPointer<AsymmetricKey> key(new DsaPrivateKey(
            shared_key.GetModulus(), shared_key.GetSubgroupOrder(),
            shared_key.GetGenerator()));
      keys->AddKey(client->GetId().ToString(), key->GetPublicKey());

      ClientPointer cs = MakeSession<ClientSession>(
            client, key, keys, create_round);
      sessions.clients.append(cs);
      sessions.private_keys[client->GetId().ToString()] = key;

      QSharedPointer<BufferSink> sink(new BufferSink());
      sessions.sinks.append(sink);

      QSharedPointer<SignalSink> ssink(new SignalSink());
      sessions.signal_sinks.append(ssink);

      QSharedPointer<SinkMultiplexer> sinkm(new SinkMultiplexer());
      sessions.sink_multiplexers.append(sinkm);
      sinkm->AddSink(sink);
      sinkm->AddSink(ssink);

      cs->SetSink(sinkm.data());
    }
    
    return sessions;
  }

  void StartSessions(const Sessions &sessions)
  {
    foreach(const ServerPointer &ss, sessions.servers) {
      ss->Start();
    }

    foreach(const ClientPointer &cs, sessions.clients) {
      cs->Start();
    }
  }

  void StartRound(const Sessions &sessions)
  {
    SignalCounter counter;
    foreach(const ServerPointer &ss, sessions.servers) {
      QObject::connect(ss.data(),
          SIGNAL(RoundStarting(const QSharedPointer<Anonymity::Round> &)),
          &counter, SLOT(Counter()));
    }

    foreach(const ClientPointer &cs, sessions.clients) {
      QObject::connect(cs.data(),
          SIGNAL(RoundStarting(const QSharedPointer<Anonymity::Round> &)),
          &counter, SLOT(Counter()));
    }

    RunUntil(counter, sessions.servers.count() + sessions.clients.count());
  }

  void CompleteRound(const Sessions &sessions)
  {
    SignalCounter counter;
    foreach(const ServerPointer &ss, sessions.servers) {
      QObject::connect(ss.data(),
          SIGNAL(RoundFinished(const QSharedPointer<Anonymity::Round> &)),
          &counter, SLOT(Counter()));
    }

    foreach(const ClientPointer &cs, sessions.clients) {
      QObject::connect(cs.data(),
          SIGNAL(RoundFinished(const QSharedPointer<Anonymity::Round> &)),
          &counter, SLOT(Counter()));
    }

    RunUntil(counter, sessions.servers.count() + sessions.clients.count());
  }

  void StopSessions(const Sessions &sessions)
  {
    foreach(const ServerPointer &ss, sessions.servers) {
      ss->Stop("Finished");
    }

    foreach(const ClientPointer &cs, sessions.clients) {
      cs->Stop("Finished");
    }
  }
  
  void SendTest(const Sessions &sessions)
  {
    qDebug() << "Starting SendTest";
    QList<QByteArray> messages;
    CryptoRandom rand;

    foreach(const QSharedPointer<BufferSink> &sink, sessions.sinks) {
      sink->Clear();
    }

    SignalCounter sc;
    foreach(const QSharedPointer<SignalSink> &ssink, sessions.signal_sinks) {
      QObject::connect(ssink.data(),
          SIGNAL(IncomingData(const QByteArray &)),
          &sc,
          SLOT(Counter()));
    }

    foreach(const ClientPointer &cs, sessions.clients) {
      QByteArray msg(64, 0);
      rand.GenerateBlock(msg);
      messages.append(msg);
      cs->Send(msg);


    }

    RunUntil(sc, sessions.clients.size() * (sessions.clients.size() + sessions.servers.size()));

    foreach(const QSharedPointer<BufferSink> &sink, sessions.sinks) {
      ASSERT_EQ(messages.size(), sink->Count());

      qDebug() << sessions.sinks.indexOf(sink);
      for(int idx = 0; idx < sink->Count(); idx++) {
        ASSERT_TRUE(messages.contains(sink->At(idx).second));
        qDebug() << "Sink value" << idx;
      }
    }
    qDebug() << "Finished SendTest";
  }

  void SFTSendTest(const Sessions &sessions, int numOnline, int numServers, int numClients)
  {
    qDebug() << "Starting SFTSendTest";
    QList<QByteArray> messages;
    //CryptoRandom rand;

    foreach(const QSharedPointer<BufferSink> &sink, sessions.sinks) {
      sink->Clear();
    }

    qDebug() << "number of sinks: " << sessions.sinks.count();
    qDebug() << "number of clients: " << sessions.clients.count();

    SignalCounter sc;
    foreach(const QSharedPointer<SignalSink> &ssink, sessions.signal_sinks) {
      QObject::connect(ssink.data(),
          SIGNAL(IncomingData(const QByteArray &)),
          &sc,
          SLOT(Counter()));
    }

    QString expectedOutput = "";

    foreach(const ClientPointer &cs, sessions.clients) {
        int clientIndex = sessions.clients.indexOf(cs);
        char letter = 'a';
        QString exchangeMsg = "";
        exchangeMsg.append(QChar(letter + clientIndex));
        exchangeMsg.append(QChar(letter + clientIndex + 1));
        exchangeMsg.append(QChar(letter + clientIndex + 2));

        QVariant variant(exchangeMsg);
        QByteArray data;
        QDataStream stream(&data,QIODevice::WriteOnly);
        stream << variant;
        QByteArray toSend;
        toSend.append(data);        
        cs->Send(toSend);
        qDebug() << "Sending: " << toSend.replace('\0', ' ');
        expectedOutput.append(exchangeMsg);
    }

    /*
    int numOnline = 0;
    for (int i = 0; i < sessions.network.second.count(); i++)
    {
        OverlayPointer op_client = sessions.network.second[i];
        int numServers = op_client->GetConnectionTable().GetConnections().length();

        //All servers that have a connection to a client still are online
        numOnline += numServers;

        //If the client still has a connection to a server, then it is online
        if (numServers > 0)
        {
            numOnline++;
        }
    }*/

    qDebug() << "Number of online nodes: " << numOnline;

    qDebug() << "Expected output is: " << expectedOutput;

    for (int i = 0; i < sessions.clients.length(); i++) {
        QVariant variant(expectedOutput);
        QByteArray data;
        QDataStream stream(&data,QIODevice::WriteOnly);
        stream << variant;

        messages.append(data); //All clients are expecting the same thing at the end
    }

    //Each client sends a message
    qDebug() << "Start run until";
    qDebug() << "Signal count: " << sc.GetCount();


    //How do I get connections from here?
    qDebug() << RunUntil(sc, numOnline); //TODO: Hardcoded for now


    qDebug() << "End run until";
    qDebug() << "Signal count: " << sc.GetCount();

    foreach(const QSharedPointer<BufferSink> &sink, sessions.sinks) {
      //ASSERT_EQ(numOnline, sink->Count());
      for(int idx = 0; idx < sink->Count(); idx++) {
          QByteArray temp(sink->At(idx).second); //Don't want to be modifiying sink->At(idx).second, so make a copy
          temp.replace('\0', ' ');
          qDebug() << "Sink value"  << "(" << numServers << ", " << numClients << ")"  << idx << temp;
          //ASSERT_TRUE(messages.contains(sink->At(idx).second));
      }
    }
    qDebug() << "Finished SendTest";
  }

  void SFTDisconnectServer(Sessions &sessions, bool hard, QList<int> *disconnectedServers, int numOnline[1])
  {
    qDebug() << "Disconnecting server" << hard;
    qDebug() << "Time for message exchange: Disconnecting server";
    qDebug() << "Time to change view: Disconnecting server";

    int server_count = sessions.servers.count();
    CryptoRandom rand;
    int idx;

    if (server_count == disconnectedServers->count())
    {
        qDebug() << "All servers already disconnected!!";
        return;
    }

    //Want to guarantee a new idx every time
    while (true)
    {
        idx = rand.GetInt(0, server_count);
        if (!disconnectedServers->contains(idx))
        {
            disconnectedServers->append(idx);
            break;
        }

    }
    OverlayPointer op_disc = sessions.network.first[idx];

    if(hard) {
      op_disc->Stop();
      sessions.servers[idx]->Stop();

      //Need to erase the server's existence
      qDebug() << "Before, servers : " << sessions.servers.count();



      sessions.sinks.removeAt(idx);
      sessions.signal_sinks.removeAt(idx);
      sessions.sink_multiplexers.removeAt(idx);

      //Cannot get rid of the public key! (or the private key for that matter)
      //sessions.keys.
      //sessions.private_keys.remove(sessions.servers[idx]->GetOverlay()->GetId().ToString());

      sessions.servers.removeAt(idx);
      sessions.network.first.removeAt(idx);
      qDebug() << op_disc->Stopped();
      qDebug() << "After, servers : " << sessions.servers.count();

      /*
      foreach(const ClientPointer &cs, sessions.clients) {
          qDebug() << cs->;
      }*/



      /*
      // This will need to be adjusted if we support offline servers
      Time::GetInstance().IncrementVirtualClock(60000);
      Timer::GetInstance().VirtualRun();

      OverlayPointer op(new Overlay(op_disc->GetId(),
            op_disc->GetLocalEndpoints(),
            op_disc->GetRemoteEndpoints(),
            op_disc->GetServerIds()));
      op->SetSharedPointer(op);
      sessions.network.first[idx] = op;
      ServerPointer ss = MakeSession<ServerSession>(
            op, sessions.private_keys[op->GetId().ToString()],
            sessions.keys, sessions.create_round);
      sessions.servers[idx] = ss;
      ss->SetSink(sessions.sink_multiplexers[idx].data());

      op->Start();
      ss->Start();*/
    } else {

      //MODIFIED: Disconnect a select server from all other servers
      int disc_count = server_count;
      QHash<int, bool> disced;
      disced[idx] = true;
      while(disced.size() < disc_count) {          
        int to_disc = rand.GetInt(0, server_count);
        if(disced.contains(to_disc)) {
          continue;
        }
        disced[to_disc] = true;
        qDebug() << "SFT Disconnected edge (" << idx << ", " << to_disc << ")";
        Id remote = sessions.network.first[to_disc]->GetId();
        op_disc->GetConnectionTable().GetConnection(remote)->Disconnect();
        //op_disc->GetConnectionTable().RemoveConnection(op_disc->GetConnectionTable().GetConnection(remote));
      }

      //Server is now disconnected
      numOnline[0]--;


      //TODO: Also disconnect the server from each of its clients
      Id remote = sessions.network.first[idx]->GetId();
      for (int i = 0; i < sessions.network.second.count(); i++)
      {
          OverlayPointer op_client = sessions.network.second[i];

          //If the client has a connection to the server, disconnect!
          if (op_client->GetConnectionTable().GetConnection(remote) != 0)
          {
              op_client->GetConnectionTable().GetConnection(remote)->Disconnect();
              //All of the server's clients are disconnected, because we are assuming only 1 server
              numOnline[0]--;
          }
      }

    }

    qDebug() << "Disconnecting done";
    //StartRound(sessions);
    //qDebug() << "Round started after disconnection";
  }

  void DisconnectServer(Sessions &sessions, bool hard)
   {
     qDebug() << "Disconnecting server" << hard;

     int server_count = sessions.servers.count();
     CryptoRandom rand;
     int idx = rand.GetInt(0, server_count);
     OverlayPointer op_disc = sessions.network.first[idx];

     if(hard) {
       op_disc->Stop();
       sessions.servers[idx]->Stop();
       // This will need to be adjusted if we support offline servers
       Time::GetInstance().IncrementVirtualClock(60000);
       Timer::GetInstance().VirtualRun();

       OverlayPointer op(new Overlay(op_disc->GetId(),
             op_disc->GetLocalEndpoints(),
             op_disc->GetRemoteEndpoints(),
             op_disc->GetServerIds()));
       op->SetSharedPointer(op);
       sessions.network.first[idx] = op;
       ServerPointer ss = MakeSession<ServerSession>(
             op, sessions.private_keys[op->GetId().ToString()],
             sessions.keys, sessions.create_round);
       sessions.servers[idx] = ss;
       ss->SetSink(sessions.sink_multiplexers[idx].data());

       op->Start();
       ss->Start();
     } else {
       // 1 for the node itself and 1 for at least another peer
       int disc_count = qMax(2, rand.GetInt(0, server_count));
       QHash<int, bool> disced;
       disced[idx] = true;
       while(disced.size() < disc_count) {
         int to_disc = rand.GetInt(0, server_count);
         if(disced.contains(to_disc)) {
           continue;
         }
         disced[to_disc] = true;
         Id remote = sessions.network.first[to_disc]->GetId();
         op_disc->GetConnectionTable().GetConnection(remote)->Disconnect();
       }
     }

     qDebug() << "Disconnecting done";
   }

  TEST(Session, Servers)
  {
    Timer::GetInstance().UseVirtualTime();
    ConnectionManager::UseTimer = false;
    OverlayNetwork net = ConstructOverlay(10, 0);
    VerifyStoppedNetwork(net);
    StartNetwork(net);
    VerifyNetwork(net);

    Sessions sessions = BuildSessions(net);
    qDebug() << "Starting sessions...";
    StartSessions(sessions);
    StartRound(sessions);
    SendTest(sessions);
    SendTest(sessions);
    DisconnectServer(sessions, true);
    SendTest(sessions);
    DisconnectServer(sessions, false);
    SendTest(sessions);
    SendTest(sessions);
    StopSessions(sessions);

    StopNetwork(sessions.network);
    VerifyStoppedNetwork(sessions.network);
    ConnectionManager::UseTimer = true;
  }

  TEST(Session, ClientsServer)
  {
    Timer::GetInstance().UseVirtualTime();
    ConnectionManager::UseTimer = false;
    OverlayNetwork net = ConstructOverlay(1, 10);
    VerifyStoppedNetwork(net);
    StartNetwork(net);
    VerifyNetwork(net);

    Sessions sessions = BuildSessions(net);
    qDebug() << "Starting sessions...";
    StartSessions(sessions);
    StartRound(sessions);
    SendTest(sessions);
    SendTest(sessions);
    DisconnectServer(sessions, true);
    SendTest(sessions);
    SendTest(sessions);
    StopSessions(sessions);

    StopNetwork(sessions.network);
    VerifyStoppedNetwork(sessions.network);
    ConnectionManager::UseTimer = true;
  }

  TEST(Session, ClientsServers)
  {
    Timer::GetInstance().UseVirtualTime();
    ConnectionManager::UseTimer = false;
    OverlayNetwork net = ConstructOverlay(10, 100);
    VerifyStoppedNetwork(net);
    StartNetwork(net);
    VerifyNetwork(net);

    Sessions sessions = BuildSessions(net);
    qDebug() << "Starting sessions...";
    StartSessions(sessions);
    StartRound(sessions);
    SendTest(sessions);
    SendTest(sessions);
    DisconnectServer(sessions, true);
    SendTest(sessions);
    DisconnectServer(sessions, false);
    SendTest(sessions);
    SendTest(sessions);
    StopSessions(sessions);

    StopNetwork(sessions.network);
    VerifyStoppedNetwork(sessions.network);
    ConnectionManager::UseTimer = true;
  }
}
}
