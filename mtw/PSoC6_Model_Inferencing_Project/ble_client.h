#ifndef BLE_CLIENT_H_
#define BLE_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

bool cmd_char_uuid_found;
bool set_char_uuid_found;
cy_ble_gatt_db_attr_handle_t gatt_cmd_attrHandle;
cy_ble_gatt_db_attr_handle_t gatt_set_attrHandle;

void ble_init(void);
void ble_process_events(void);

#ifdef __cplusplus
}
#endif


#endif /* BLE_CLIENT_H_ */
