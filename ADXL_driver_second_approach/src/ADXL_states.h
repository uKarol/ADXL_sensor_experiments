#include "state_machine.h"

typedef enum
{
    ADXL_EVT_EXTI_IRQ = SPECIAL_EVT_END,
    ADXL_EVT_START_STREAM_REQUEST,
    ADXL_EVT_STOP_STREAM_REQUEST,
	ADXL_EVT_COM_TX_COMPLETED, 
	ADXL_EVT_COM_RX_COMPLETED,
    ADXL_EVT_COMM_TIMEOUT, 
    ADXL_EVT_TIMEOUT,
    ADXL_EVT_BUFFER_RELEASE_REQ,
};

StateRetVal ADXL_InitializedHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
// halted ralated substates
StateRetVal ADXL_HaltedIdleHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
StateRetVal ADXL_HaltedSettingPowerCTLHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
StateRetVal ADXL_HaltedWaitingHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
// flushing related substates 
StateRetVal ADXL_FlushingCheckFIFOHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
StateRetVal ADXL_FlushingStreamFlushHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);

// waiting related substates
StateRetVal ADXL_WaitingIdleHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
StateRetVal ADXL_WaitingCheckINTHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
StateRetVal ADXL_WaitingCheckFIFOHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);

// in progress substate
StateRetVal ADXL_InProgressHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
// completed substate
StateRetVal ADXL_CompletedHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);

// stopping state handlers
StateRetVal ADXL_StoppingWaitingHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
StateRetVal ADXL_StoppingResetingPowerCTLHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);

// special handlers for unexpected IRQ
StateRetVal ADXL_UnexpectedIRQWaitingHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);
StateRetVal ADXL_UnexpectedIRQCheckingIntStatusHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);

// special state for error handling
StateRetVal ADXL_ErrorHandler(MyStateMachine_t *ctx, FsmEvent_t *evt);

StateMachineRet_t ADXL_InitializeStateMachine(uint8_t fifo_samples_num);

void ADXL_ProcessEvent(FsmEvent_t *evt);