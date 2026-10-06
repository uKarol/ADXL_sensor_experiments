#include "unity.h"

#include "state_machine.h"
#include "ADXL_states.h"

#include "mock_ADXL_SensorCom.h"
#include "mock_evt_timer.h"


extern MyState_t ADXL_Initialized;
// halted ralated substates
extern MyState_t ADXL_HaltedIdle;
extern MyState_t ADXL_HaltedSettingPowerCTL;
extern MyState_t ADXL_HaltedWaiting;
// flushing related substates 
extern MyState_t ADXL_FlushingCheckFIFO;
extern MyState_t ADXL_FlushingStreamFlush;

// waiting related substates
extern MyState_t ADXL_WaitingIdle;
extern MyState_t ADXL_WaitingCheckINT;
extern MyState_t ADXL_WaitingCheckFIFO;

// in progress substate
extern MyState_t ADXL_InProgress;
// completed substate
extern MyState_t ADXL_Completed;

// stopping state handlers
extern MyState_t ADXL_StoppingWaiting;
extern MyState_t ADXL_StoppingResetingPowerCTL;

// special handlers for unexpected IRQ
extern MyState_t ADXL_UnexpectedIRQWaiting;
extern MyState_t ADXL_UnexpectedIRQCheckingIntStatus;

// special state for error handling
extern MyState_t ADXL_Error;

extern MyStateMachine_t ADXL_MainFSM;

void test_init()
{
    TEST_ASSERT_EQUAL(STATE_MACHINE_OK, ADXL_InitializeStateMachine(6));
    TEST_ASSERT_EQUAL(ADXL_Initialized.depth, 0);
    TEST_ASSERT_EQUAL(ADXL_HaltedIdle.depth, 1);
    TEST_ASSERT_EQUAL(&ADXL_Initialized, ADXL_HaltedIdle.parent);
}

void test_process_event_to_waiting_state()
{
    uint8_t value_out = POWER_CTL_MEASURE;
    FsmEvent_t evt = {ADXL_EVT_START_STREAM_REQUEST, NULL};

    ADXL_WriteRegNonBlocking_ExpectAndReturn(POWER_CTL, NULL, ADXL_ERR_NO_ERROR);
    ADXL_WriteRegNonBlocking_IgnoreArg_DataIn();
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_HaltedSettingPowerCTL, ADXL_MainFSM.current_state);

    evt.user_event = ADXL_EVT_COM_TX_COMPLETED;
    ADXL_ReadRegNonBlocking_ExpectAndReturn(POWER_CTL, NULL, ADXL_ERR_NO_ERROR);
    ADXL_ReadRegNonBlocking_ReturnThruPtr_pValueOut(&value_out);
    ADXL_ReadRegNonBlocking_IgnoreArg_pValueOut();
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_HaltedSettingPowerCTL, ADXL_MainFSM.current_state);

    evt.user_event = ADXL_EVT_COM_RX_COMPLETED;
    EvtTimerStart_ExpectAndReturn(0, 100, EVT_TIMER_OK);
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_HaltedWaiting, ADXL_MainFSM.current_state);

    evt.user_event = ADXL_EVT_TIMEOUT;
    uint8_t readout_temp = 6;
    ADXL_ReadRegNonBlocking_ExpectAndReturn(FIFO_STATUS, NULL, ADXL_ERR_NO_ERROR);
    ADXL_ReadRegNonBlocking_IgnoreArg_pValueOut();
    ADXL_ReadRegNonBlocking_ReturnThruPtr_pValueOut(&readout_temp);
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_FlushingCheckFIFO, ADXL_MainFSM.current_state);

    evt.user_event = ADXL_EVT_COM_RX_COMPLETED;
    ADXL_ReadMultipleRegsNonBlocking_ExpectAndReturn(DATAX0_REG, NULL, ONE_SAMPLE_SIZE, ADXL_ERR_NO_ERROR);
    ADXL_ReadMultipleRegsNonBlocking_IgnoreArg_pValueOut();
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_FlushingStreamFlush, ADXL_MainFSM.current_state);

    // next steps are flushing of adxl x6

    for(int i = 0; i < 5; i++)
    {
        evt.user_event = ADXL_EVT_COM_RX_COMPLETED;
        ADXL_ReadMultipleRegsNonBlocking_ExpectAndReturn(DATAX0_REG, NULL, ONE_SAMPLE_SIZE, ADXL_ERR_NO_ERROR);
        ADXL_ReadMultipleRegsNonBlocking_IgnoreArg_pValueOut();
        ADXL_ProcessEvent(&evt);
        TEST_ASSERT_EQUAL(&ADXL_FlushingStreamFlush, ADXL_MainFSM.current_state);
    }

    evt.user_event = ADXL_EVT_COM_RX_COMPLETED;
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_WaitingIdle, ADXL_MainFSM.current_state);


    evt.user_event = ADXL_EVT_EXTI_IRQ;
    value_out = ADXL_INT_ENABLE_WATERMARK;
    ADXL_ReadRegNonBlocking_ExpectAndReturn(INT_SOURCE, NULL, ADXL_ERR_NO_ERROR);
    ADXL_ReadRegNonBlocking_ReturnThruPtr_pValueOut(&value_out);
    ADXL_ReadRegNonBlocking_IgnoreArg_pValueOut();
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_WaitingCheckINT, ADXL_MainFSM.current_state);


    evt.user_event = ADXL_EVT_COM_RX_COMPLETED;
    value_out = 6;
    ADXL_ReadRegNonBlocking_ExpectAndReturn(FIFO_STATUS, NULL, ADXL_ERR_NO_ERROR);
    ADXL_ReadRegNonBlocking_ReturnThruPtr_pValueOut(&value_out);
    ADXL_ReadRegNonBlocking_IgnoreArg_pValueOut();
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_WaitingCheckFIFO, ADXL_MainFSM.current_state);

    // exit from waiting
    evt.user_event = ADXL_EVT_COM_RX_COMPLETED;
    ADXL_ReadMultipleRegsNonBlocking_ExpectAndReturn(DATAX0_REG, NULL, ONE_SAMPLE_SIZE, ADXL_ERR_NO_ERROR);
    ADXL_ReadMultipleRegsNonBlocking_IgnoreArg_pValueOut();
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_InProgress, ADXL_MainFSM.current_state);

    // in progress test start 
    for(int i = 0; i < 5; i++)
    {
        evt.user_event = ADXL_EVT_COM_RX_COMPLETED;
        ADXL_ReadMultipleRegsNonBlocking_ExpectAndReturn(DATAX0_REG, NULL, ONE_SAMPLE_SIZE, ADXL_ERR_NO_ERROR);
        ADXL_ReadMultipleRegsNonBlocking_IgnoreArg_pValueOut();
        ADXL_ProcessEvent(&evt);
        TEST_ASSERT_EQUAL(&ADXL_InProgress, ADXL_MainFSM.current_state);
    }

    //entry to completed
    evt.user_event = ADXL_EVT_COM_RX_COMPLETED;
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_Completed, ADXL_MainFSM.current_state);

    // release buffer
    evt.user_event = ADXL_EVT_BUFFER_RELEASE_REQ;
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_WaitingIdle, ADXL_MainFSM.current_state);

    // to stop 
    evt.user_event = ADXL_EVT_STOP_STREAM_REQUEST;
    ADXL_Com_GetCurrentOperation_ExpectAndReturn(ADXL_OP_NO_OPERATION);
    ADXL_WriteRegNonBlocking_ExpectAndReturn(POWER_CTL, NULL, ADXL_ERR_NO_ERROR);
    ADXL_WriteRegNonBlocking_IgnoreArg_DataIn();
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_StoppingResetingPowerCTL, ADXL_MainFSM.current_state);

    //stopping
    evt.user_event = ADXL_EVT_COM_TX_COMPLETED;
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_HaltedIdle, ADXL_MainFSM.current_state);
    
}

void test_unexpected_irq_in_waiting_state()
{
    // we start from state halted
    FsmEvent_t evt;
    evt.user_event = ADXL_EVT_EXTI_IRQ;
    uint8_t value_out = ADXL_INT_SOURCE_OVERRUN;
    ADXL_Com_GetCurrentOperation_ExpectAndReturn(ADXL_READ_SINGLE_REG);
    ADXL_ProcessEvent(&evt);    
    TEST_ASSERT_EQUAL(&ADXL_UnexpectedIRQWaiting, ADXL_MainFSM.current_state);

    // go to check int status
    evt.user_event = ADXL_EVT_COM_TX_COMPLETED;
    ADXL_ReadRegNonBlocking_ExpectAndReturn(INT_SOURCE, NULL, ADXL_ERR_NO_ERROR);
    ADXL_ReadRegNonBlocking_ReturnThruPtr_pValueOut(&value_out);
    ADXL_ReadRegNonBlocking_IgnoreArg_pValueOut();
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_UnexpectedIRQCheckingIntStatus, ADXL_MainFSM.current_state);

    // go to error
    evt.user_event = ADXL_EVT_COM_RX_COMPLETED;
    ADXL_ProcessEvent(&evt);
    TEST_ASSERT_EQUAL(&ADXL_Error, ADXL_MainFSM.current_state);

}

void setUp(void)
{
}

void tearDown(void)
{
}
