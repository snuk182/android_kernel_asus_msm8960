/*
         bat facade interface include file

*/
#ifndef __AXC_BATTERY_SERVICE_H__
#define __AXC_BATTERY_SERVICE_H__
#include "AXI_BatteryServiceFacade.h"
#include "../fsm/AXC_Charging_FSM.h"
#include "../gauge/axi_gauge.h"
#include <linux/workqueue.h>
#include "AXC_BatteryServiceTest.h"
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include "../charger/axi_charger.h" 
#include "../capfilter/axi_cap_filter.h"

#define DEFAULT_ASUSBAT_POLLING_INTERVAL (0)

typedef struct AXC_BatteryService {
    AXI_BatteryServiceFacade miParent;
    bool mbInit;
    bool IsFirstForceResume;
    AXI_BatteryServiceFacadeCallback *callback;
    int A66_capacity;
    int Pad_capacity;
    int Dock_capacity;
    time_t ForceSavedTime;	
    time_t savedTime;
    time_t P02_savedTime;
    time_t Dock_savedTime;
    bool BatteryService_IsCable;
    bool BatteryService_IsCharging;
    bool BatteryService_IsFULL;
    bool BatteryService_IsBatLow;
    bool isMainBatteryChargingDone;
    bool IsFirstAskCap;
    bool IsFirstAskCable;
    bool HasCableBeforeSuspend;
    bool IsResumeUpdate;
    bool IsResumeMahUpdate;
    bool IsCalculateCapOngoing;
    bool P02_IsCable;
    bool P02_IsCharging;
    bool P02_IsFULL;
    bool P02_IsBatLow;
    bool P02_IsFirstAskCap;
    bool P02_HasCableBeforeSuspend;
    bool P02_IsResumeUpdate;
    int  P02_ChgStatusBeforeSuspend;
    int P02_MaxMahBeforeSuspend;
    bool Dock_IsCable;
    bool Dock_IsCharging;
    bool Dock_IsFULL;
    bool Dock_IsBatLow;
    bool Dock_IsFirstAskCap;
    bool Dock_HasCableBeforeSuspend;
    bool Dock_IsResumeUpdate;
    int  Dock_ChgStatusBeforeSuspend;
    int Dock_MaxMahBeforeSuspend;
    int IsSuspend;
    bool IsDockExtCableIn;
    bool IsDockExtChgIn;
    bool IsDockInitReady;
    AXI_Gauge_Callback gaugeCallback;
    AXI_Gauge_Callback P02gaugeCallback;
    AXI_Gauge_Callback DockgaugeCallback;
    AXE_Charger_Type chargerType;
    AXI_Gauge *gauge;
    AXI_Gauge *P02gauge;
    AXI_Gauge *Dockgauge;
    AXI_Cap_Filter *gpCapFilterA66;
    AXI_Cap_Filter *gpCapFilterP02;
    AXI_Cap_Filter *gpCapFilterDock;
    time_t defaultPollingInterval;
    struct workqueue_struct *BatteryServiceCapUpdateQueue;
    struct delayed_work BatteryServiceUpdateWorker;
    struct delayed_work BatRtcReadyWorker;
    struct delayed_work BatEocWorker;
    struct delayed_work BatEcAcWorker;
    struct delayed_work ResumeWorker;
    struct delayed_work CableOffWorker;
    struct delayed_work SetRTCWorker;
    struct delayed_work PadAlarmResumeWorker;//Eason: dynamic set Pad alarm	
    struct delayed_work SetBatLowRTCWorker;
    struct delayed_work SetCableInRTCWorker;
	AXI_Charging_FSM_Callback fsmCallback;
	AXC_Charging_FSM *fsm;
    AXE_Charging_State fsmState;
    AXC_BatteryServiceTest test;
    struct mutex main_lock;
    struct mutex filter_lock;
    struct wake_lock cap_wake_lock;
    struct wake_lock resume_wake_lock;
    struct wake_lock cableoff_wake_lock;
    AXI_ChargerStateChangeNotifier gChargerStateChangeNotifier;
}AXC_BatteryService;


#endif //__AXC_BATTERY_SERVICE_H__


