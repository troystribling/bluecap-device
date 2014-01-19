#ifndef _BLUE_CAP_PERIPHERAL_H
#define _BLUE_CAP_PERIPHERAL_H

#include "nordic/lib_aci.h"

class BlueCapPeripheral {

public:

  BlueCapPeripheral(uint8_t reqn, uint8_t rdyn);
  BlueCapPeripheral(uint8_t         reqn,
                    uint8_t         rdyn,
                    hal_aci_data_t* messages,
                    int             messagesCount);
  BlueCapPeripheral(uint8_t                       reqn,
                    uint8_t                       rdyn,
                    hal_aci_data_t*               messages,
                    int                           messagesCount,
                    services_pipe_type_mapping_t* mapping,
                    int                           mappingCount);
  ~BlueCapPeripheral();

  void begin();
  virtual void loop(){listen();};
  virtual bool areAllPipesAvailable() = 0;

  bool connected(void);
  bool sendAck(const uint8_t pipe);
  bool sendNack(const uint8_t pipe, const uint8_t error_code);
  bool sendData(uint8_t pipe, uint8_t *value, uint8_t size);
  bool getBatteryLevel();

  void setServicePipeTypeMapping(services_pipe_type_mapping_t* mapping, int count);
  void setSetUpMessages(hal_aci_data_t* messages, int count);

protected:

  virtual void didReceiveData(uint8_t characteristicId, uint8_t* data, uint8_t length){};
  virtual void didDisconnect(){};
  virtual void didConnect(){};
  virtual void didStartAdvertising(){};
  virtual void didReceiveError(uint8_t pipe, uint8_t errorCode){};

  bool isPipeAvailable(uint8_t pipe);

private:

  services_pipe_type_mapping_t*   servicesPipeTypeMapping;
  int                             numberOfPipes;
  hal_aci_data_t*                 setUpMessages;
  int                             numberOfSetupMessages;
  bool                            isConnected;
  bool                            ack;
  bool                            timingChangeDone;
  uint8_t                         reqnPin;
  uint8_t                         rdynPin;
  uint8_t*                        rxPipes;
  aci_state_t                     aciState;
  hal_aci_evt_t                   aciData;

private:

  void init(uint8_t                       reqn,
            uint8_t                       rdyn,
            hal_aci_data_t*               messages,
            int                           messagesCount,
            services_pipe_type_mapping_t* mapping,
            int                           mappingCount);

  void listen();
  void incrementCredit();
  void decrementCredit();
};

#endif

