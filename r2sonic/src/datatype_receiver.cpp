#include "datatype_receiver.hpp"

NS_HEAD

template<typename PKT_T>
DatatypeReceiver<PKT_T>::DatatypeReceiver(r2sonic::R2SonicNode *node_ptr){
  (void)static_cast<packets::Packet*>((PKT_T*)0);  // ensure template type is a child of packet.
  node_ptr_ = node_ptr;

  // Initialize raw data logger with device-specific name
  std::string device_name;
  if (std::is_same_v<PKT_T, packets::BTH0>) {
    device_name = "r2sonic_bth0";
  } else if (std::is_same_v<PKT_T, packets::AID0>) {
    device_name = "r2sonic_aid0";
  } else {
    device_name = "r2sonic_unknown";
  }
  
  raw_logger_ = std::make_unique<RawDataLogger>(device_name, node_ptr->getParams().raw_log_directory);
}
template<typename PKT_T>
void DatatypeReceiver<PKT_T>::receiveImpl(const boost::system::error_code &error, size_t bytes_transferred){
  
  // Log raw data before processing
  if (raw_logger_ && bytes_transferred > 0) {
    raw_logger_->write_data(startBit(), bytes_transferred);
  }

  PKT_T packet(startBit());
  if(packet.isType()){
    node_ptr_->publish(packet);
  }
  return;
}

template class DatatypeReceiver<packets::BTH0>;
template class DatatypeReceiver<packets::AID0>;

NS_FOOT
