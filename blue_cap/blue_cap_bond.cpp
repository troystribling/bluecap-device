#include <SPI.h>
#include <EEPROM.h>
#include "nordic/boards.h"
#include "nordic/lib_aci.h"
#include "nordic/aci_setup.h"
#include "dlog.h"

#include "blue_cap_peripheral.h"

#define BOND_DOES_NOT_EXIST_AT_INDEX    0xF0

BlueCapPeripheral::BlueCapBond::BlueCapBond() {
}

void BlueCapPeripheral::BlueCapBond::init(uint16_t _eepromOffset, uint16_t _maxBonds, uint8_t _index) {
  eepromOffset = _eepromOffset;
  index = _index;
  maxBonds = _maxBonds;
  if (status() == 0x00) {
    bonded = false;
  } else {
    bonded = true;
  }
}

void  BlueCapPeripheral::BlueCapBond::clearBondData() {
    DLOG(F("clearBondData eepromOffset, index, maxBonds, offset:"));
    DLOG(eepromOffset, HEX);
    DLOG(index, HEX);
    DLOG(maxBonds, HEX);
    DLOG(offset(), HEX);
    EEPROM.write(offset(), 0x00);
}

void  BlueCapPeripheral::BlueCapBond::setup(aci_state_t* aciState) {
  DLOG(F("Initial eepromOffset, index, maxBonds and offset:"));
  DLOG(eepromOffset, HEX);
  DLOG(index, DEC);
  DLOG(maxBonds, HEX);
  DLOG(offset());
  for (int i = 0; i < maxBonds; i++) {
    DLOG(F("Header status, size and address:"));
    DLOG(EEPROM.read(eepromOffset+i), HEX);
    DLOG(EEPROM.read(eepromOffset+i+1), DEC);
    DLOG(eepromOffset+i, HEX);
  }
  aciState->bonded = ACI_BOND_STATUS_FAILED;
}

bool BlueCapPeripheral::BlueCapBond::restoreAndAdvertise(aci_state_t* aciState) {
  bool result = true;
  if (bonded) {
    DLOG(F("Previous Bond present. Restoring"));
    if (ACI_STATUS_TRANSACTION_COMPLETE == restoreBondData(aciState)) {
      lib_aci_connect(100/* in seconds */, 0x0020 /* advertising interval 20ms*/);
      DLOG(F("Bond restored successfully: Waiting for connection"));
    }
    else {
      DLOG(F("Bond restore failed. Delete the bond and try again."));
      result = false;
    }
  } else if (ACI_BOND_STATUS_SUCCESS != aciState->bonded) {
    lib_aci_bond(180/* in seconds */, 0x0050 /* advertising interval 50ms*/);
    DLOG(F("Advertising started : Waiting for connection and bonding"));
  } else {
    lib_aci_connect(100/* in seconds */, 0x0020 /* advertising interval 20ms*/);
    DLOG(F("Already bonded : Advertising started : Waiting for connection"));
  }
  return result;
}

void BlueCapPeripheral::BlueCapBond::writeIfBondedAndAdvertise(aci_state_t* aciState, aci_evt_t* aciEvt) {
  if (ACI_BOND_STATUS_SUCCESS == aciState->bonded) {
    aciState->bonded = ACI_BOND_STATUS_FAILED;
    if (ACI_STATUS_EXTENDED == aciEvt->params.disconnected.aci_status) {
      if (!bonded) {
        if (readAndWriteBondData(aciState)) {
          bonded = true;
          DLOG(F("Bond data read and store successful"));
        } else {
          DLOG(F("Bond data read and store failed"));
        }
      }
    }
    lib_aci_connect(180/* in seconds */, 0x0100 /* advertising interval 100ms*/);
    DLOG(F("Using existing bond stored in EEPROM."));
    DLOG(F("Advertising started. Waiting for connection"));
  } else {
    lib_aci_bond(180/* in seconds */, 0x0050 /* advertising interval 50ms*/);
    DLOG(F("Advertising started : Waiting for connection and bonding"));
  }
}

// private
aci_status_code_t  BlueCapPeripheral::BlueCapBond::restoreBondData(aci_state_t* aciState) {
  aci_evt_t *aciEvt;
  uint16_t addr = readBondDataOffset();
  uint8_t numDynMsgs = status() & 0x7F;
  hal_aci_data_t aciCmd;

  DLOG(F("restoreBondData dynamic messages:"));
  DLOG(numDynMsgs, HEX);

  while(1) {

    DLOG(F("Restore messages:"));
    DLOG(numDynMsgs);
    addr = readBondData(&aciCmd, addr);

    DLOG(F("Send restore command"));
    if (!hal_aci_tl_send(&aciCmd)) {
      DLOG(F("restoreBondData: failed"));
      return ACI_STATUS_ERROR_INTERNAL;
    }

    while (1) {
      if (lib_aci_event_get(aciState, &aciData)) {
        aciEvt = &aciData.evt;
        DLOG(F("Event recieved with opcode:"));
        DLOG(aciEvt->evt_opcode, HEX);
        if (ACI_EVT_CMD_RSP != aciEvt->evt_opcode) {
            DLOG(F("restoreBondData: failed with error: 0x"));
            DLOG(aciEvt->evt_opcode, HEX);
            return ACI_STATUS_ERROR_INTERNAL;
        } else {
          numDynMsgs--;
          DLOG(F("Command status:"));
          DLOG(aciEvt->params.cmd_rsp.cmd_status, HEX);
          if (ACI_STATUS_TRANSACTION_COMPLETE == aciEvt->params.cmd_rsp.cmd_status) {
            bonded = true;
            aciState->bonded = ACI_BOND_STATUS_SUCCESS;
            DLOG(F("Restore of bond data completed successfully"));
            return ACI_STATUS_TRANSACTION_COMPLETE;
          } else if (ACI_STATUS_TRANSACTION_CONTINUE == aciEvt->params.cmd_rsp.cmd_status) {
            DLOG(F("Restore next bond data message"));
            break;
          } else if (0 >= numDynMsgs) {
            DLOG(F("Restore of bond data failed with too many messages"));
            return ACI_STATUS_ERROR_INTERNAL;
          } else {
            DLOG(F("Restore of bond data failed with cmd_status:"));
            DLOG(aciEvt->params.cmd_rsp.cmd_status, HEX);
            return ACI_STATUS_ERROR_INTERNAL;
          }
        }
      }
    }
  }
}

bool  BlueCapPeripheral::BlueCapBond::readAndWriteBondData(aci_state_t* aciState) {
  bool status = false;
  aci_evt_t* aciEvt = NULL;
  uint8_t numDynMsgs = 0;
  uint16_t addr = readBondDataOffset();

  lib_aci_read_dynamic_data();
  numDynMsgs++;

  DLOG(F("readAndWriteBondData"));

  while (1) {
    if (lib_aci_event_get(aciState, &aciData)) {
      DLOG(F("Recieved event for message:"));
      DLOG(numDynMsgs, DEC);
      aciEvt = &aciData.evt;
      if (ACI_EVT_CMD_RSP != aciEvt->evt_opcode ) {
        DLOG(F("readAndWriteBondData command response failed:"));
        DLOG(aciEvt->evt_opcode, HEX);
        status = false;
        break;
      } else if (ACI_STATUS_TRANSACTION_COMPLETE == aciEvt->params.cmd_rsp.cmd_status) {
        DLOG(F("readAndWriteBondData transaction complete"));
        addr = writeBondData(aciEvt, addr);
        writeBondDataHeader(addr, numDynMsgs);
        status = true;
        break;
      } else if (!(ACI_STATUS_TRANSACTION_CONTINUE == aciEvt->params.cmd_rsp.cmd_status)) {
        DLOG(F("readAndWriteBondData transaction failed:"));
        DLOG(aciEvt->params.cmd_rsp.cmd_status, HEX);
        clearBondData();
        status = false;
        break;
      } else {
        DLOG(F("readAndWriteBondData transaction continue"));
        addr = writeBondData(aciEvt, addr);
        lib_aci_read_dynamic_data();
        numDynMsgs++;
      }
    }
  }
  return status;
}

uint16_t  BlueCapPeripheral::BlueCapBond::writeBondData(aci_evt_t* evt, uint16_t addr) {
  DLOG(F("writeBondData"));
  DLOG(F("Message length:"));
  DLOG(evt->len - 2, HEX);
  EEPROM.write(addr, evt->len - 2);
  addr++;
  DLOG(F("Event op_code:"));
  DLOG(ACI_CMD_WRITE_DYNAMIC_DATA, HEX);
  EEPROM.write(addr, ACI_CMD_WRITE_DYNAMIC_DATA);
  addr++;
  for (uint8_t i=0; i< (evt->len-3); i++) {
    DLOG(F("message address and value:"));
    DLOG(addr, HEX);
    DLOG(evt->params.cmd_rsp.params.padding[i], HEX);
    EEPROM.write(addr, evt->params.cmd_rsp.params.padding[i]);
    addr++;
  }
  return addr;
}

uint16_t BlueCapPeripheral::BlueCapBond::readBondData(hal_aci_data_t* aciCmd, uint16_t addr) {
  DLOG(F("Start reading message"));
  uint8_t len = EEPROM.read(addr);
  addr++;
  aciCmd->buffer[0] = len;
  DLOG(F("Message len:"));
  DLOG(len, HEX);
  for (uint8_t i = 1; i <= len; i++) {
      aciCmd->buffer[i] = EEPROM.read(addr);
      DLOG(F("message address and value:"));
      DLOG(addr, HEX);
      DLOG(aciCmd->buffer[i], HEX);
      addr++;
  }
  DLOG(F("Finished reading message"));
  return addr;
}

void BlueCapPeripheral::BlueCapBond::writeBondDataHeader(uint16_t dataAddress, uint8_t numDynMsgs) {
  uint8_t dataSize = dataAddress - readBondDataOffset();
  EEPROM.write(offset(), 0x80 | numDynMsgs);
  EEPROM.write(offset() + 1, dataSize);
  DLOG(F("writeBondDataHeader dataSize, status, dataAddress, readBondDataOffset"));
  DLOG(dataSize, DEC);
  DLOG(0x80 | numDynMsgs, HEX);
  DLOG(dataAddress, DEC);
  DLOG(readBondDataOffset(), DEC);
}

uint16_t BlueCapPeripheral::BlueCapBond::readBondDataOffset() {
  uint16_t bondOffset = eepromOffset + maxBonds*BOND_HEADER_BYTES;
  for (int i = 0; i < index; i++) {
    bondOffset += EEPROM.read(eepromOffset + i*BOND_HEADER_BYTES + 1);
  }
  DLOG(F("readBondDataOffset offset:"));
  DLOG(bondOffset, DEC);
  return bondOffset;
}

uint8_t  BlueCapPeripheral::BlueCapBond::status() {
  return EEPROM.read(offset());
}

uint16_t BlueCapPeripheral::BlueCapBond::offset() {
  return eepromOffset + index*BOND_HEADER_BYTES;
}