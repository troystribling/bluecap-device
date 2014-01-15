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

  unsigned char connected(void);

  void setServicePipeTypeMapping(services_pipe_type_mapping_t* mapping, int count);
  void setSetUpMessages(hal_aci_data_t* messages, int count);

protected:

  virtual void didReceiveData(uint8_t characteristicId, uint8_t* data, uint8_t length){};
  virtual void didDisconnect(){};
  virtual void didConnect(){};
  virtual void didStartAdvertising(){};

private:

  services_pipe_type_mapping_t*   servicesPipeTypeMapping;
  int                             numberOfPipes;
  hal_aci_data_t*                 setUpMessages;
  int                             numberOfSetupMessages;
  unsigned char                   isConnected;
  unsigned char                   ack;
  unsigned char                   timingChangeDone;
  uint8_t                         reqnPin;
  uint8_t                         rdynPin;
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
};

#endif

