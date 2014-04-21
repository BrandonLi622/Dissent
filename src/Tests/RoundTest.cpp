#include "DissentTest.hpp"
#include "OverlayTest.hpp"
#include "SessionTest.hpp"
#include "SFT/SFTNullRound.hpp"

namespace Dissent {

namespace Tests {
  void TestRoundBasic(CreateRound create_round)
  {
    Timer::GetInstance().UseVirtualTime();
    ConnectionManager::UseTimer = false;
    OverlayNetwork net = ConstructOverlay(3, 10);
    VerifyStoppedNetwork(net);
    StartNetwork(net);
    VerifyNetwork(net);

    Sessions sessions = BuildSessions(net, create_round);
    qDebug() << "Starting sessions...";
    StartSessions(sessions);
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

  TEST(NeffShuffleRound, Basic)
  {
    TestRoundBasic(TCreateRound<NeffShuffleRound>);
  }

  TEST(CSDCNetRound, Basic)
  {
    TestRoundBasic(TCreateDCNetRound<CSDCNetRound, NullRound>);
  }

  TEST(NullRound, Basic)
  {
      CreateRound create_round = TCreateRound<NullRound>;
      Timer::GetInstance().UseVirtualTime();
      ConnectionManager::UseTimer = false;
      OverlayNetwork net = ConstructOverlay(5, 15);
      VerifyStoppedNetwork(net);
      StartNetwork(net);
      VerifyNetwork(net);

      Sessions sessions = BuildSessions(net, create_round);
      qDebug() << "Starting sessions...";
      StartSessions(sessions);

      SendTest(sessions);
      DisconnectServer(sessions, true);
      SendTest(sessions);

      StopSessions(sessions);
      StopNetwork(sessions.network);
      VerifyStoppedNetwork(sessions.network);
      ConnectionManager::UseTimer = true;
  }

  TEST(SFTNullRound, Basic)
  {
      int numServers = 10;
      int numClients = 20;

      CreateRound create_round = TCreateRound<SFT::SFTNullRound>;
      //TestRoundBasic(TCreateRound<SFT::SFTNullRound>);
      Timer::GetInstance().UseVirtualTime();
      ConnectionManager::UseTimer = false;
      //OverlayNetwork net = ConstructOverlay(8, 15);
      OverlayNetwork net = ConstructOverlay(numServers, numClients);

      //Works with 15 servers and 80 clients


      VerifyStoppedNetwork(net);
      StartNetwork(net);
      VerifyNetwork(net);

      Sessions sessions = BuildSessions(net, create_round);
      qDebug() << "Starting sessions...";
      StartSessions(sessions);


      QList<int> *disconnectedServers = new QList<int>(); //Need this so we don't pick the same server twice by accident
      int onlineNodes[1] = {numServers + numClients};

      qDebug() << "Number of online: " << onlineNodes[0];

      SFTSendTest(sessions, onlineNodes[0]);
      //SFTDisconnectServer(sessions, true);
      SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
      SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
      SFTSendTest(sessions, onlineNodes[0]);
      SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
      SFTSendTest(sessions, onlineNodes[0]);

      StopSessions(sessions);
      StopNetwork(sessions.network);
      VerifyStoppedNetwork(sessions.network);
      ConnectionManager::UseTimer = true;
  }
}
}
