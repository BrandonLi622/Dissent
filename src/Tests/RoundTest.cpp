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

      SFTSendTest(sessions, onlineNodes[0], numServers, numClients);
      //SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
      //SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
      //SFTSendTest(sessions, onlineNodes[0]);
      SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
      SFTSendTest(sessions, onlineNodes[0], numServers - 1, onlineNodes[0] - (numServers - 1)); //the one is for the single disconnected server

      StopSessions(sessions);
      StopNetwork(sessions.network);
      VerifyStoppedNetwork(sessions.network);
      ConnectionManager::UseTimer = true;
  }
  
  TEST(SFTNullRound, ManyDisconnects)
  {
      int numServers = 6;
      int numClients = 15;

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
      SFTSendTest(sessions, onlineNodes[0], numServers, numClients);

      for (int i = 0; i <= .33 * numServers; i++)
      {
        SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
        SFTSendTest(sessions, onlineNodes[0], numServers - disconnectedServers->length(), onlineNodes[0] - (numServers - disconnectedServers->length())); //the one is for the single disconnected server
      }
      
      /*
      SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
      SFTSendTest(sessions, onlineNodes[0], numServers - disconnectedServers->length(), onlineNodes[0] - (numServers - disconnectedServers->length())); //the one is for the single disconnected server
      SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
      SFTSendTest(sessions, onlineNodes[0], numServers - disconnectedServers->length(), onlineNodes[0] - (numServers - disconnectedServers->length())); //the one is for the single disconnected server
      SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
      SFTSendTest(sessions, onlineNodes[0], numServers - disconnectedServers->length(), onlineNodes[0] - (numServers - disconnectedServers->length())); //the one is for the single disconnected server
*/
      StopSessions(sessions);
      StopNetwork(sessions.network);
      VerifyStoppedNetwork(sessions.network);
      ConnectionManager::UseTimer = true;
  }


  //For collecting statistics, we run many different rounds
  TEST(SFTNullRound, ManyRounds)
  {
      int startNumServers = 5;
      int startNumClients = 10;
      int maxNumServers = 40;
      int maxNumClients = 100;

      for (int i = startNumServers; i <= maxNumServers; i += 5)
      {
          for (int j = startNumClients; j <= maxNumClients; j++)
          {
              CreateRound create_round = TCreateRound<SFT::SFTNullRound>;
              Timer::GetInstance().UseVirtualTime();
              ConnectionManager::UseTimer = false;
              OverlayNetwork net = ConstructOverlay(i, j);

              //Works with 15 servers and 80 clients


              VerifyStoppedNetwork(net);
              StartNetwork(net);
              VerifyNetwork(net);

              Sessions sessions = BuildSessions(net, create_round);
              qDebug() << "Starting sessions...";
              StartSessions(sessions);


              QList<int> *disconnectedServers = new QList<int>(); //Need this so we don't pick the same server twice by accident
              int onlineNodes[1] = {i + j};

              qDebug() << "Number of online: " << onlineNodes[0];

              SFTSendTest(sessions, onlineNodes[0], i, j);
              //SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
              //SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
              //SFTSendTest(sessions, onlineNodes[0]);
              SFTDisconnectServer(sessions, false, disconnectedServers, onlineNodes);
              SFTSendTest(sessions, onlineNodes[0], i - disconnectedServers->length(), onlineNodes[0] - (i - disconnectedServers->length()));

              StopSessions(sessions);
              StopNetwork(sessions.network);
              VerifyStoppedNetwork(sessions.network);
              ConnectionManager::UseTimer = true;
          }
      }
  }
}
}
