#include "ADXL_states.h"
#include "ADXL_SensorCom.h"
#include "evt_timer.h"

#include "stdio.h"

MyState_t ADXL_Initialized;
// halted ralated substates
MyState_t ADXL_HaltedIdle;
MyState_t ADXL_HaltedSettingPowerCTL;
MyState_t ADXL_HaltedWaiting;
// flushing related substates 
MyState_t ADXL_FlushingCheckFIFO;
MyState_t ADXL_FlushingStreamFlush;

// waiting related substates
MyState_t ADXL_WaitingIdle;
MyState_t ADXL_WaitingCheckINT;
MyState_t ADXL_WaitingCheckFIFO;

// in progress substate
MyState_t ADXL_InProgress;
// completed substate
MyState_t ADXL_Completed;

// stopping state handlers
MyState_t ADXL_StoppingWaiting;
MyState_t ADXL_StoppingResetingPowerCTL;

// special handlers for unexpected IRQ
MyState_t ADXL_UnexpectedIRQWaiting;
MyState_t ADXL_UnexpectedIRQCheckingIntStatus;

// special state for error handling
MyState_t ADXL_Error;

uint8_t ADXL_raw_data[MAX_READOUT_SIZE];

typedef struct
{
    uint8_t adxl_data_in;
    uint8_t adxl_data_out;
    ADXL_Errors_t last_error;
    uint8_t evt_tmr_id;
    uint8_t readout_num;
    uint8_t expected_size;
    uint8_t dma_readout[6];
    uint8_t dma_out_data;
    uint8_t fifo_samples_num;
}ADXL_ContextData_t;

ADXL_ContextData_t ADXL_ContextData;

MyStateMachine_t ADXL_MainFSM;

StateMachineRet_t ADXL_InitializeStateMachine(uint8_t fifo_samples_num)
{
    if( InitState(&ADXL_Initialized, ADXL_InitializedHandler, NULL) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_HaltedIdle, ADXL_HaltedIdleHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_HaltedSettingPowerCTL, ADXL_HaltedSettingPowerCTLHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_HaltedWaiting, ADXL_HaltedWaitingHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_FlushingCheckFIFO, ADXL_FlushingCheckFIFOHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_FlushingStreamFlush, ADXL_FlushingStreamFlushHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_WaitingIdle, ADXL_WaitingIdleHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_WaitingCheckINT, ADXL_WaitingCheckINTHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_WaitingCheckFIFO, ADXL_WaitingCheckFIFOHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_InProgress, ADXL_InProgressHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_Completed, ADXL_CompletedHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_StoppingWaiting, ADXL_StoppingWaitingHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_StoppingResetingPowerCTL, ADXL_StoppingResetingPowerCTLHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_UnexpectedIRQWaiting, ADXL_UnexpectedIRQWaitingHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_UnexpectedIRQCheckingIntStatus, ADXL_UnexpectedIRQCheckingIntStatusHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    if( InitState(&ADXL_Error, ADXL_ErrorHandler, &ADXL_Initialized) != STATE_MACHINE_OK ) return STATE_MACHINE_ERROR;
    StateMachineInitialize(&ADXL_MainFSM, &ADXL_HaltedIdle);
    ADXL_MainFSM.context = (void*)&ADXL_ContextData;
    ADXL_ContextData.fifo_samples_num = fifo_samples_num;
    return STATE_MACHINE_OK;
}

StateRetVal ADXL_InitializedHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val = STATE_HANDLED;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("INITIALIZED - ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;

        case ADXL_EVT_COMM_TIMEOUT:
            ctx->next_state = &ADXL_Error;
            ret_val = STATE_TRANSITION;
            break;

        case ADXL_EVT_STOP_STREAM_REQUEST:
            if(ADXL_Com_GetCurrentOperation() == ADXL_OP_NO_OPERATION)
            {
                ctx->next_state = &ADXL_StoppingResetingPowerCTL;
            }
            else
            {
                ctx->next_state = &ADXL_StoppingWaiting;
            }
            ret_val = STATE_TRANSITION;
            break;

        case ADXL_EVT_EXTI_IRQ:
            if(ADXL_Com_GetCurrentOperation() == ADXL_OP_NO_OPERATION)
            {
                ctx->next_state = &ADXL_UnexpectedIRQCheckingIntStatus;
            }
            else
            {
                ctx->next_state = &ADXL_UnexpectedIRQWaiting;
            }
            ret_val = STATE_TRANSITION;
        break;

        case EXIT_EVT:
            ret_val = STATE_HANDLED;
        break;

        default:
            ret_val = STATE_IGNORED;
            break;
    }
    return ret_val;
}

// halted ralated substates
StateRetVal ADXL_HaltedIdleHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val = STATE_HANDLED;

    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("HALTED IDLE - ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;

        case ADXL_EVT_START_STREAM_REQUEST:
            context_data->adxl_data_in = POWER_CTL_MEASURE;
            if(ADXL_WriteRegNonBlocking(POWER_CTL, &(context_data->adxl_data_in)) == ADXL_ERR_NO_ERROR)
            {
                ctx->next_state = &ADXL_HaltedSettingPowerCTL;
            }
            else
            {
                ctx->next_state = &ADXL_Error;
            }
            ret_val = STATE_TRANSITION;
            break;

        case EXIT_EVT:
            printf("HALTED IDLE - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

        default:
            ret_val = STATE_IGNORED;
            break;

    }
    return ret_val;
}

StateRetVal ADXL_HaltedSettingPowerCTLHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("HALTED SETTING POWER CTL - ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;

        case EXIT_EVT:
            printf("HALTED SETTING POWER CTL - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

        case ADXL_EVT_COM_TX_COMPLETED:

			if(ADXL_ReadRegNonBlocking(POWER_CTL, &(context_data->adxl_data_out))!= ADXL_ERR_NO_ERROR)
			{
                ctx->next_state = &ADXL_Error;
				ret_val = STATE_TRANSITION;
				context_data->last_error = ADXL_ERR_COMMUNICATION_LOST;
			}
			break;

		case ADXL_EVT_COM_RX_COMPLETED:
			if(context_data->adxl_data_out == POWER_CTL_MEASURE)
			{
				if(EvtTimerStart(context_data->evt_tmr_id, 100) == EVT_TIMER_OK)
				{
					ctx->next_state = &ADXL_HaltedWaiting;
				}
				else
				{
                    ctx->next_state = &ADXL_Error;
					context_data->last_error = ADXL_ERR_TIMER_FAILURE;
				}
			}
			else
			{
                ctx->next_state = &ADXL_Error;
			}
            ret_val = STATE_TRANSITION;
            break;


        default:
            ret_val = STATE_IGNORED;
            break;

    }
    return ret_val;
}

StateRetVal ADXL_HaltedWaitingHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("HALTED WAITING - ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;

        case EXIT_EVT:
            printf("HALTED WAITING - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

		case ADXL_EVT_TIMEOUT:
            ctx->next_state = &ADXL_FlushingCheckFIFO;
            ret_val = STATE_TRANSITION;
            break;

        default:
            ret_val = STATE_IGNORED;
            break;
    }
    return ret_val;
}

// flushing related substates 
StateRetVal ADXL_FlushingCheckFIFOHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;

    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("FlushingCheckFIFO - ENTRY EVT\n");
    		if(ADXL_ReadRegNonBlocking(FIFO_STATUS, &(context_data->dma_out_data)) == ADXL_ERR_NO_ERROR )
			{
				context_data->expected_size = 0;
				context_data->readout_num = 0;
			}
			else
			{
                // to do post error to queue
			}
            ret_val = STATE_HANDLED;
        break;

		case ADXL_EVT_COM_RX_COMPLETED:
			context_data->expected_size = (context_data->dma_out_data & FIFO_ENTRIES_BIT_MSK);
			if(context_data->expected_size == 0)
			{
				ctx->next_state = &ADXL_WaitingIdle;
			}
			else
			{
                ctx->next_state = &ADXL_FlushingStreamFlush;
			}
            ret_val = STATE_TRANSITION;
			break;

        case EXIT_EVT:
            printf("FlushingCheckFIFO - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;
        default:
            ret_val = STATE_IGNORED;
            break;

    }
    return ret_val;
}

StateRetVal ADXL_FlushingStreamFlushHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_FlushingStreamFlush - ENTRY EVT\n");
			if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, context_data->dma_readout, ONE_SAMPLE_SIZE) != ADXL_ERR_NO_ERROR)
			{
				context_data->last_error = ADXL_ERR_COMMUNICATION_LOST;
                // to do add event to queue
			}
            ret_val = STATE_HANDLED;
        break;

        case EXIT_EVT:
            printf("ADXL_FlushingStreamFlush - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

		case ADXL_EVT_COM_RX_COMPLETED:
			context_data->readout_num++;

			if(context_data->readout_num == context_data->expected_size)
			{
                ctx->next_state = &ADXL_WaitingIdle;
                ret_val = STATE_TRANSITION;
			}
			else
			{
                ret_val = STATE_HANDLED;
				if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, context_data->dma_readout, ONE_SAMPLE_SIZE) != ADXL_ERR_NO_ERROR)
				{
					ctx->next_state = &ADXL_Error;
					context_data->last_error = ADXL_ERR_COMMUNICATION_LOST;
                    ret_val = STATE_TRANSITION;
				}
			}
			break;

        default:
            ret_val = STATE_IGNORED;
            break;
    }
    return ret_val;
}

// waiting related substates
StateRetVal ADXL_WaitingIdleHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_WaitingIdle - ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;

		case ADXL_EVT_EXTI_IRQ:
			/* code */
			if(ADXL_ReadRegNonBlocking(INT_SOURCE, &(context_data->dma_out_data)) == ADXL_ERR_NO_ERROR )
			{
				ctx->next_state = &ADXL_WaitingCheckINT;
			}
			else
			{
				ctx->next_state = &ADXL_Error;
				context_data->last_error = ADXL_ERR_DMA_PROBLEM;
			}
            ret_val = STATE_TRANSITION;
		break;
    
        case EXIT_EVT:
            printf("ADXL_WaitingIdle - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;
        default:
            ret_val = STATE_IGNORED;
            break;
    }
    return ret_val;
}

StateRetVal ADXL_WaitingCheckINTHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_WaitingCheckINTHandler- ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;
    
        case EXIT_EVT:
            printf("ADXL_WaitingCheckINTHandler - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;
        
		case ADXL_EVT_COM_RX_COMPLETED:
			/* code */
			if(context_data->dma_out_data & ADXL_INT_ENABLE_OVERRUN)
			{ 
                context_data->last_error = ADXL_ERR_OVERRUN;
                ctx->next_state = &ADXL_Error;
                ret_val = STATE_TRANSITION;
			}
			else if(context_data->dma_out_data & ADXL_INT_ENABLE_WATERMARK)
			{
				if( ADXL_ReadRegNonBlocking(FIFO_STATUS, &(context_data->dma_out_data)) == ADXL_ERR_NO_ERROR )
				{
                    ctx->next_state = &ADXL_WaitingCheckFIFO;
				}
				else
				{
					context_data->last_error = ADXL_ERR_DMA_PROBLEM;
                    ctx->next_state = &ADXL_Error;
				}
                ret_val = STATE_TRANSITION;
			}
			break;

         default:
            ret_val = STATE_IGNORED;
            break;
    }
    return ret_val;
}

StateRetVal ADXL_WaitingCheckFIFOHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_WaitingCheckFIFOHandler- ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;
    
        case EXIT_EVT:
            printf("ADXL_WaitingCheckFIFOHandler - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

		case ADXL_EVT_COM_RX_COMPLETED:
			/* code */
			if((context_data->dma_out_data & FIFO_ENTRIES_BIT_MSK) >= context_data->expected_size)
			{
                if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, ADXL_raw_data, ONE_SAMPLE_SIZE) == ADXL_ERR_NO_ERROR)
                {
                    ctx->next_state = &ADXL_InProgress;
                    ret_val = STATE_TRANSITION;
                }
                else
                {
                    ctx->next_state = &ADXL_Error;
                    ret_val = STATE_TRANSITION;
                }

			}
			break;

         default:
            ret_val = STATE_IGNORED;
            break;
    }
    return ret_val;

}

// in progress substate
StateRetVal ADXL_InProgressHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_InProgressHandler - ENTRY EVT\n");
            context_data->readout_num = 0;
            ret_val = STATE_HANDLED;
        break;
    
        case EXIT_EVT:
            printf("ADXL_InProgressHandler - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

		case ADXL_EVT_COM_RX_COMPLETED:
			if(context_data->readout_num == (context_data->fifo_samples_num-1))
			{
				context_data->readout_num = 0;
				ctx->next_state = &ADXL_Completed;
                ret_val = STATE_TRANSITION;
			}
			else
			{
				context_data->readout_num++;
				if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, &(ADXL_raw_data[context_data->readout_num * ONE_SAMPLE_SIZE]), ONE_SAMPLE_SIZE) != ADXL_ERR_NO_ERROR)
				{
                    ctx->next_state = &ADXL_Error;
					context_data->last_error = ADXL_ERR_DMA_PROBLEM;
                    ret_val = STATE_TRANSITION;
				}
			}
			break;

        default:
            ret_val = STATE_IGNORED;
            break;
    }
    return ret_val;
}
// completed substate
StateRetVal ADXL_CompletedHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_CompletedHandler - ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;
    
        case EXIT_EVT:
            printf("ADXL_CompletedHandler - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

		case ADXL_EVT_BUFFER_RELEASE_REQ:
            ctx->next_state = &ADXL_WaitingIdle;
            ret_val = STATE_TRANSITION;
        break;
        
        default:
            ret_val = STATE_IGNORED;
        break;
    }
    return ret_val;
}

// stopping state handlers
StateRetVal ADXL_StoppingWaitingHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_StoppingWaitingHandler - ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;
    
        case EXIT_EVT:
            printf("ADXL_StoppingWaitingHandler - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

    	case ADXL_EVT_COM_TX_COMPLETED: // fallthrough
		case ADXL_EVT_COM_RX_COMPLETED:
			ctx->next_state = &ADXL_StoppingResetingPowerCTL;
            ret_val = STATE_TRANSITION;
		break;
        
        default:
            ret_val = STATE_IGNORED;
        break;
    }
    return ret_val;
}

StateRetVal ADXL_StoppingResetingPowerCTLHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_StoppingResetingPowerCTLHandler - ENTRY EVT\n");
            if(ADXL_WriteRegNonBlocking(POWER_CTL, &(context_data->adxl_data_in)) != ADXL_ERR_NO_ERROR)
			{
                // to do handle error
				context_data->last_error = ADXL_ERR_COMMUNICATION_LOST;
			}
            ret_val = STATE_HANDLED;
        break;
        
        case ADXL_EVT_COM_TX_COMPLETED:
            // to do notify upper layer
            ctx->next_state = &ADXL_HaltedIdle;
            ret_val = STATE_TRANSITION;
		break;

        case EXIT_EVT:
            printf("ADXL_StoppingResetingPowerCTLHandler - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;
        
        default:
            ret_val = STATE_IGNORED;
        break;
    }
    return ret_val;
}

// special handlers for unexpected IRQ
StateRetVal ADXL_UnexpectedIRQWaitingHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_StoppingWaitingHandler - ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;
    
        case EXIT_EVT:
            printf("ADXL_StoppingWaitingHandler - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

    	case ADXL_EVT_COM_TX_COMPLETED: // fallthrough
		case ADXL_EVT_COM_RX_COMPLETED:
			ctx->next_state = &ADXL_UnexpectedIRQCheckingIntStatus;
            ret_val = STATE_TRANSITION;
		break;
        
        default:
            ret_val = STATE_IGNORED;
        break;
    }
    return ret_val;
}

StateRetVal ADXL_UnexpectedIRQCheckingIntStatusHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_StoppingWaitingHandler - ENTRY EVT\n");
            if(ADXL_ReadRegNonBlocking(INT_SOURCE, &context_data->adxl_data_out) != ADXL_ERR_NO_ERROR)
			{
                // to do add error handling
			}
            ret_val = STATE_HANDLED;
        break;
    
        case EXIT_EVT:
            printf("ADXL_StoppingWaitingHandler - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;
        
		case ADXL_EVT_COM_RX_COMPLETED: // to do - notify upper laye 
			if(context_data->adxl_data_out & ADXL_INT_SOURCE_OVERRUN)
			{
                context_data->last_error = ADXL_ERR_OVERRUN;
			}
			else if(context_data->adxl_data_out & ADXL_INT_SOURCE_WATERMARK)
			{
                context_data->last_error = ADXL_ERR_UNEXPECTED_WATERMARK;
			}
			else
			{
                context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			}
            ctx->next_state = &ADXL_Error;
            ret_val = STATE_TRANSITION;

			break;

        default:
            ret_val = STATE_IGNORED;
        break;
    }
    return ret_val;
}

// special state for error handling
StateRetVal ADXL_ErrorHandler(MyStateMachine_t *ctx, FsmEvent_t *evt)
{
    StateRetVal ret_val;
    ADXL_ContextData_t* context_data = (ADXL_ContextData_t*)ctx->context;
    switch(evt->user_event)
    {
        case ENTRY_EVT:
            printf("ADXL_ErrorHandler - ENTRY EVT\n");
            ret_val = STATE_HANDLED;
        break;
    
        case EXIT_EVT:
            printf("ADXL_ErrorHandler - EXIT EVT\n");
            ret_val = STATE_HANDLED;
        break;

        default:
            ret_val = STATE_HANDLED;
        break;
    }
    return ret_val;

}

void ADXL_ProcessEvent(FsmEvent_t *evt)
{
    StateMachine_ProcessEvent(&ADXL_MainFSM, evt);
}