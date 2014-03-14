#ifndef DISSENT_SESSION_SESSION_STATE_H_GUARD
#define DISSENT_SESSION_SESSION_STATE_H_GUARD

#include <QObject>

#include "Anonymity/Round.hpp"
#include "Connections/Id.hpp"
#include "Messaging/ISender.hpp"
#include "Messaging/State.hpp"
#include "Messaging/StateMachine.hpp"
#include "Utils/QRunTimeError.hpp"

#include "SessionSharedState.hpp"
#include "SessionMessage.hpp"

namespace Dissent {
namespace Session {
  class SessionStates : QObject {
    Q_OBJECT
    Q_ENUMS(Names)

    public:
      enum Names {
        Offline = 0,
        WaitingForServers,
        Init,
        Enlist,
        Agree,
        WaitingForServer,
        Queuing,
        Registering,
        ListExchange,
        VerifyList,
        Communicating
      };

      /** 
       * Converts a MessageType into a QString
       * @param mt value to convert
       */
      static QString StateTypeToString(qint8 type)
      {
        int index = staticMetaObject.indexOfEnumerator("Names");
        return staticMetaObject.enumerator(index).valueToKey(type);
      }
  };

  class SessionState : public Messaging::State {
    public:
      explicit SessionState(const QSharedPointer<Messaging::StateData> &data,
          SessionStates::Names state, SessionMessage::Names msg_type) :
        Messaging::State(data, state, msg_type)
      {
      }

      virtual ProcessResult ProcessPacket(
          const QSharedPointer<Messaging::ISender> &,
          const QSharedPointer<Messaging::Message> &)
      {
        return Messaging::State::NoChange;
      }

      /**
       * An incoming connection
       */
      virtual ProcessResult HandleConnection(const Connections::Id &)
      {
        return Messaging::State::NoChange;
      }

      /**
       * A lost connection
       */
      virtual ProcessResult HandleDisconnection(const Connections::Id &)
      {
        return Messaging::State::NoChange;
      }

      virtual QString ToString() const { return SessionStates::StateTypeToString(GetState()); }

    protected:
      QSharedPointer<SessionSharedState> GetSharedState() const
      {
        return GetStateData().dynamicCast<SessionSharedState>();
      }

    private:
      virtual bool StorePacket(const QSharedPointer<Messaging::Message> &) const
      {
        return false;
      }

      virtual bool RestartPacket(const QSharedPointer<Messaging::Message> &msg) const
      {
        // We could move all the verification logic here too
        return (msg->GetMessageType() == SessionMessage::ServerStop);
      }
  };

  inline QDebug operator<<(QDebug dbg, const SessionState &state)
  {
    dbg.nospace() << state.ToString();
    return dbg.space();
  }

  inline QDebug operator<<(QDebug dbg, const QSharedPointer<SessionState> &state)
  {
    dbg.nospace() << state->ToString();
    return dbg.space();
  }

  inline QDebug operator<<(QDebug dbg, SessionState *state)
  {
    dbg.nospace() << state->ToString();
    return dbg.space();
  }

  class SessionStateMachine : public Messaging::StateMachine {
    public:
      explicit SessionStateMachine(const QSharedPointer<SessionSharedState> &data) :
        StateMachine(data)
      {
      }

      virtual ~SessionStateMachine() {}

      virtual void HandleConnection(const Connections::Id &connector)
      {
        ResultProcessor(GetCurrentState().dynamicCast<SessionState>()->HandleConnection(connector));
      }

      /**
       * A lost connection
       */
      virtual void HandleDisconnection(const Connections::Id &disconnector)
      {
        ResultProcessor(GetCurrentState().dynamicCast<SessionState>()->HandleDisconnection(disconnector));
      }

    private:
      virtual void PrintError(const QSharedPointer<Messaging::ISender> &from,
          const Utils::QRunTimeError &err) const
      {
        QSharedPointer<SessionSharedState> state =
          GetStateData().dynamicCast<SessionSharedState>();
        qWarning() << "From: " << from <<
              "To:" << state->GetOverlay()->GetId() <<
              "In:" << GetCurrentState().dynamicCast<SessionState>() <<
              "Error:" << err.What();
      }
  };
}
}

#endif