#include "r2sonic_node.hpp"

NS_HEAD

template<typename T>
void setupParam(T * variable,rclcpp::Node *node , std::string topic, T initial_val){
  node->declare_parameter(topic, initial_val);
  *variable =
      node->get_parameter(topic).get_parameter_value().get<T>();
}

// a explicit overload for string is required for casting to work correctly
void setupParam(std::string * variable,rclcpp::Node *node , std::string topic, std::string initial_val){
  setupParam<std::string>(variable,node,topic,initial_val);
}

void R2SonicNode::Parameters::init(rclcpp::Node *node){
  setupParam(&topics.detections,node,"topics/detections","~/detections");
  setupParam(&topics.bth0,node,"topics/bth0","~/raw/bth0");
  setupParam(&topics.aid0,node,"topics/aid0","~/raw/aid0");
  setupParam(&topics.acoustic_image,node,"topics/acoustic_image","~/acoustic_image");
  setupParam(&ports.bathy,node,"ports/bathy",65500);
  setupParam(&ports.acoustic_image,node,"ports/acoustic_image" ,65503);
  setupParam(&sonar_ip,node,"sonar_ip","10.0.0.86");
  setupParam(&interface_ip,node,"interface_ip","0.0.0.0");
  setupParam(&tx_frame_id,node,"tx_frame_id","r2sonic_tx");
  setupParam(&rx_frame_id,node,"rx_frame_id","r2sonic_rx");
  setupParam(&raw_log_directory,node,"raw_log_directory","/home/sph/ws/raw_logs/");

  RCLCPP_INFO(node->get_logger(), "Listening on interface  : %s", interface_ip.c_str());
  RCLCPP_INFO(node->get_logger(), "Sending sonar comands on: %s", sonar_ip.c_str());
}

template <typename T>
bool send_udp_message(packets::CmdPacket<T> message, const std::string& destination_ip,
            const unsigned short port) {

  using namespace boost::asio;
  io_service io_service;
  ip::udp::socket socket(io_service);
  auto remote = ip::udp::endpoint(ip::address::from_string(destination_ip), port);
  try {
    socket.open(boost::asio::ip::udp::v4());
    socket.send_to(buffer(reinterpret_cast<char*>(&message),
                          sizeof(packets::CmdPacket<T>)),
                          remote);

  } catch (const boost::system::system_error& ex) {
    return false;
  }
  return true;
}

R2SonicNode::R2SonicNode():
  Node("r2sonic")
{
  parameters_.init(this);

  if(shouldAdvertise(getParams().topics.detections)){
    msg_buffer_.dectections.pub =
        this->create_publisher<acoustic_msgs::msg::SonarDetections>(parameters_.topics.detections,100);
  }

  if(shouldAdvertise(getParams().topics.bth0)){
    msg_buffer_.bth0.pub =
        this->create_publisher<r2sonic_interfaces::msg::RawPacket>(parameters_.topics.bth0,100);
  }

  if(shouldAdvertise(getParams().topics.aid0)){
    msg_buffer_.aid0.pub =
        this->create_publisher<r2sonic_interfaces::msg::RawPacket>(parameters_.topics.aid0,100);
  }

  if(shouldAdvertise(getParams().topics.acoustic_image)){
    msg_buffer_.acoustic_image.pub =
        this->create_publisher<acoustic_msgs::msg::RawSonarImage>(parameters_.topics.acoustic_image,100);
  }

}

//void R2SonicNode::publish(packets::Packet &r2_packet){
//  publish(reinterpret_cast<packets::BTH0>(&r2_packet));
//  publish(reinterpret_cast<packets::AID0>(&r2_packet));
//}

void R2SonicNode::publish(packets::BTH0 &r2_packet){
  if(!r2_packet.isType()){
    return;
  }

  msg_buffer_.dectections.lock();
  msg_buffer_.bth0.lock();

  if(shouldPublish(msg_buffer_.dectections.pub)){
    conversions::bth02SonarDetections(&msg_buffer_.dectections.msg,r2_packet);
    msg_buffer_.dectections.msg.header.frame_id = getParams().tx_frame_id;
    msg_buffer_.dectections.msg.ping_info.rx_frame_id = getParams().rx_frame_id;
    msg_buffer_.dectections.pub->publish(msg_buffer_.dectections.msg);
  }

  if(shouldPublish(msg_buffer_.bth0.pub)){
    conversions::packet2RawPacket(&msg_buffer_.bth0.msg,&r2_packet);
    msg_buffer_.bth0.msg.frame_id = getParams().tx_frame_id;
    msg_buffer_.bth0.pub->publish(msg_buffer_.bth0.msg);
  }

  msg_buffer_.dectections.unlock();
  msg_buffer_.bth0.unlock();
}

void R2SonicNode::publish(packets::AID0 &aid0_packet){
  if(!aid0_packet.isType()){
    return;
  }
  msg_buffer_.aid0.lock();
  msg_buffer_.acoustic_image.lock();

  auto ping_no = aid0_packet.getPingNo();

  if(shouldPublish(msg_buffer_.aid0.pub)){
    conversions::packet2RawPacket(&msg_buffer_.aid0.msg,&aid0_packet);
    msg_buffer_.aid0.msg.frame_id = getParams().tx_frame_id;
    msg_buffer_.aid0.pub->publish(msg_buffer_.aid0.msg);
  }

  if(shouldPublish(msg_buffer_.acoustic_image.pub)){

    auto msg = &msg_buffer_.acoustic_image.msg[ping_no];
    if(conversions::aid02RawAcousticImage(msg,aid0_packet)){
      msg->header.frame_id = getParams().tx_frame_id;
      msg->ping_info.rx_frame_id = getParams().rx_frame_id;
      msg_buffer_.acoustic_image.pub->publish(*msg);
      msg_buffer_.acoustic_image.msg.erase(ping_no);
      cleanMsgMap(&msg_buffer_.acoustic_image.msg, ping_no);
    }
  }

  msg_buffer_.aid0.unlock();
  msg_buffer_.acoustic_image.unlock();
}

template <typename T>
void R2SonicNode::cleanMsgMap(msgMap<T> *msg_map, u32 ping_no){
  auto iter = msg_map->begin();
  auto end_itr = msg_map->end();
  for(; iter != end_itr; ) {
    auto item_ping_no = iter->first;
    if (ping_no - 10 > item_ping_no) {
        iter = msg_map->erase(iter);
    } else {
        ++iter;
    }
  }
}

bool R2SonicNode::shouldAdvertise(std::string topic){
  return topic != "";
}

bool R2SonicNode::shouldPublish(rclcpp::PublisherBase::SharedPtr pub){
  if(!pub){
    return false;
  }
  return true;
  return pub->get_subscription_count() > 0;
}

NS_FOOT
