/* Generated from Yell extensions to the Telepathy spec



 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TPY_CONTENT_REMOVAL_REASON_UNKNOWN = 0,
    TPY_CONTENT_REMOVAL_REASON_USER_REQUESTED = 1,
    TPY_CONTENT_REMOVAL_REASON_ERROR = 2,
    TPY_CONTENT_REMOVAL_REASON_UNSUPPORTED = 3,
} TpyContentRemovalReason;
#define NUM_TPY_CONTENT_REMOVAL_REASONS (3+1)

typedef enum {
    TPY_CALL_CONTENT_DISPOSITION_NONE = 0,
    TPY_CALL_CONTENT_DISPOSITION_INITIAL = 1,
} TpyCallContentDisposition;
#define NUM_TPY_CALL_CONTENT_DISPOSITIONS (1+1)

typedef enum {
    TPY_CALL_CONTENT_PACKETIZATION_TYPE_RTP = 0,
    TPY_CALL_CONTENT_PACKETIZATION_TYPE_RAW = 1,
    TPY_CALL_CONTENT_PACKETIZATION_TYPE_MSN_WEBCAM = 2,
} TpyCallContentPacketizationType;
#define NUM_TPY_CALL_CONTENT_PACKETIZATION_TYPES (2+1)

typedef enum {
    TPY_SENDING_STATE_NONE = 0,
    TPY_SENDING_STATE_PENDING_SEND = 1,
    TPY_SENDING_STATE_SENDING = 2,
    TPY_SENDING_STATE_PENDING_STOP_SENDING = 3,
} TpySendingState;
#define NUM_TPY_SENDING_STATES (3+1)

typedef enum {
    TPY_STREAM_COMPONENT_UNKNOWN = 0,
    TPY_STREAM_COMPONENT_DATA = 1,
    TPY_STREAM_COMPONENT_CONTROL = 2,
} TpyStreamComponent;
#define NUM_TPY_STREAM_COMPONENTS (2+1)

typedef enum {
    TPY_STREAM_TRANSPORT_TYPE_UNKNOWN = 0,
    TPY_STREAM_TRANSPORT_TYPE_RAW_UDP = 1,
    TPY_STREAM_TRANSPORT_TYPE_ICE = 2,
    TPY_STREAM_TRANSPORT_TYPE_GTALK_P2P = 3,
    TPY_STREAM_TRANSPORT_TYPE_WLM_2009 = 4,
    TPY_STREAM_TRANSPORT_TYPE_SHM = 5,
    TPY_STREAM_TRANSPORT_TYPE_MULTICAST = 6,
} TpyStreamTransportType;
#define NUM_TPY_STREAM_TRANSPORT_TYPES (6+1)

typedef enum {
    TPY_CALL_STATE_UNKNOWN = 0,
    TPY_CALL_STATE_PENDING_INITIATOR = 1,
    TPY_CALL_STATE_PENDING_RECEIVER = 2,
    TPY_CALL_STATE_ACCEPTED = 3,
    TPY_CALL_STATE_ENDED = 4,
} TpyCallState;
#define NUM_TPY_CALL_STATES (4+1)

typedef enum {
    TPY_CALL_FLAG_LOCALLY_RINGING = 1,
    TPY_CALL_FLAG_QUEUED = 2,
    TPY_CALL_FLAG_LOCALLY_HELD = 4,
    TPY_CALL_FLAG_FORWARDED = 8,
    TPY_CALL_FLAG_IN_PROGRESS = 16,
    TPY_CALL_FLAG_CLEARING = 32,
    TPY_CALL_FLAG_MUTED = 64,
} TpyCallFlags;

typedef enum {
    TPY_CALL_STATE_CHANGE_REASON_UNKNOWN = 0,
    TPY_CALL_STATE_CHANGE_REASON_USER_REQUESTED = 1,
    TPY_CALL_STATE_CHANGE_REASON_FORWARDED = 2,
    TPY_CALL_STATE_CHANGE_REASON_NO_ANSWER = 3,
} TpyCallStateChangeReason;
#define NUM_TPY_CALL_STATE_CHANGE_REASONS (3+1)

typedef enum {
    TPY_CALL_MEMBER_FLAG_RINGING = 1,
    TPY_CALL_MEMBER_FLAG_HELD = 2,
    TPY_CALL_MEMBER_FLAG_CONFERENCE_HOST = 4,
} TpyCallMemberFlags;


#ifdef __cplusplus
}
#endif
