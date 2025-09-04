#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/buildings-module.h"
#include "ns3/flow-monitor-module.h"
#include "multi-hop-lora-app.h" //include our custom application header

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("MultiHopLoraSimulation");

int main(int argc, char *argv[])
{
    //--- command line parameters ---//
    std::string scenario = "obstructed";
    double simulationTime = 3700.0; //approx 1 hours as in paper
    uint32_t numPackets = 184;

    CommandLine cmd(__FILE__);
    cmd.AddValue("scenario", "Select scenario: unobstructed or obstructed", scenario);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("numPackets", "Total number of packets to be sent by the source", numPackets);
    cmd.Parse(argc, argv);

    //base network configuration
    LogComponentEnable("MultiHopLoraApp", LOG_LEVEL_INFO);
    LogComponentEnable("MultiHopLoraSimulation", LOG_LEVEL_INFO);

    //create nodes: 1 source, 2 repeaters, 1 gateway
    NodeContainer nodes;
    nodes.Create(4);
    Ptr<Node> sourceNode = nodes.Get(0);
    Ptr<Node> repeater1Node = nodes.Get(1);
    Ptr<Node> repeater2Node = nodes.Get(2);
    Ptr<Node> gatewayNode = nodes.Get(3);

    //---lora PHY and channel configuration---//
    Ptr<LoraChannel> channel = CreateObject<LoraChannel>();
    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);

    LoraHelper helper = LoraHelper();
    helper.SetPhyHelper(phyHelper);

    //all nodes in the multi-hop network use the same PHY parameters
    //we are not using LORAWAN MAC, so we install devices directly
    NetDeviceContainer devices = helper.Install(nodes);

    //set ccconsistent SF and frequency for all devices
    //note: the lorawan module primarily configure device via the MAC layer
    //for this custom application, we ensure the PHY iis set up for broadcast
    //the default settings for LoraPHYHelper will typically suffice if a single
    //channel is used. we can enforce settings if needed
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<LoraNetDevice> loraDevice = devices.Get(i)->GetObject<LoraNetDevice>();
        Ptr<LoraPhy> phy = loraDevice->GetPhy();
        //the lorawan module does not expose aa simple SetSpreadingFactor on the helper
        //we rely on the default channel/PHY settings for this simulation
        //all devices on the same channel will be able to communicate
        phy->SetTxPower(12.0); //set tx power to 12 dBm as per paper
    }

    //---mobility and positioning---//
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    if (scenario == "unobstructed")
    {
        NS_LOG_INFO("Configuring UNOBSTRUCTED scenario");
        //place nodes in a line, 1.8m apart as in fig.5
        sourceNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0, 0, 1.5));
        repeater1Node->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(1.8, 0, 1.5));
        repeater2Node->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(3.6, 0, 1.5));
        gatewayNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(5.4, 0, 1.5));

        //use a simple propagation model for LoS
        channel->SetPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());
    }
    else // Default to obstructed scenario
    {
        NS_LOG_INFO("Configuring OBSTRUCTED scenario");
        //place nodes according to fig.6
        //node 1 (source) @ 2nd floor, south wing
        sourceNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0, 0, 6.0)); //z=6 for 2nd floor
        //node 2 (repeater) @ 2nd floor, middle wing
        repeater1Node->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(50, 0, 6.0));
        //node 3 (repeater) @ 3rd floor, north wing
        repeater2Node->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(100, 20, 9.0)); //z=9 for 3rd floor
        //gateway @ 3rd floor, north end
        gatewayNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(100, 0, 9.0));

        //create building to obstruct the signal
        double x_min = 10.0;
        double x_max = 90.0;
        double y_min = -10.0;
        double y_max = 10.0;
        double z_min = 0.0;
        double z_max = 12.0; //a tall building
        Ptr<Building> b = CreateObject<Building>();
        b->SetBoundaries(Box(x_min, x_max, y_min, y_max, z_min, z_max));
        b->SetBuildingType(Building::Office);
        b->SetExtWallsType(Building::ConcreteWithWindows);
        b->SetNFloors(3);
        b->SetNRoomsX(10);
        b->SetNRoomsY(4);

        BuildingsHelper::Install(nodes);
        BuildingsHelper::MakeMobilityModelConsistent();

        //use buildingsPropagationLossModel for NLoS
        channel->SetPropagationLossModel(CreateObject<HybridBuildingsPropagationLossModel>());
    }

    //---Aplication Deployment---//
    double packetInterval = simulationTime / numPackets;
    uint32_t packetSize = 32; //assumed payload size

    //install aappliication on all nodes
    ApplicationContainer apps;

    //source node (ID = 0, LGW = 4)
    Ptr<MultiHopLoraApp> sourceApp = CreateObject<MultiHopLoraApp>();
    sourceNode->AddApplication(sourceApp);
    sourceApp->Setup(0, 4, false, true, packetInterval, packetSize);
    apps.Add(sourceApp);

    // Repeater 1 (ID 1, LGw=3)
    Ptr<MultiHopLoraApp> repeater1App = CreateObject<MultiHopLoraApp>();
    repeater1Node->AddApplication(repeater1App);
    repeater1App->Setup(1, 3, false, false, packetInterval, packetSize);
    apps.Add(repeater1App);

    // Repeater 2 (ID 2, LGw=2)
    Ptr<MultiHopLoraApp> repeater2App = CreateObject<MultiHopLoraApp>();
    repeater2Node->AddApplication(repeater2App);
    repeater2App->Setup(2, 2, false, false, packetInterval, packetSize);
    apps.Add(repeater2App);

    // Gateway Node (ID 3, LGw=1)
    Ptr<MultiHopLoraApp> gatewayApp = CreateObject<MultiHopLoraApp>();
    gatewayNode->AddApplication(gatewayApp);
    gatewayApp->Setup(3, 1, true, false, packetInterval, packetSize);
    apps.Add(gatewayApp);

    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime - 1.0));

    // --- Data Collection (FlowMonitor) ---//
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    //--- Run Simulation --//
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    //--- Performance Analysis ---//
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("multi-hop-lora-results.xml", true, true);

    Simulator::Destroy();

    NS_LOG_INFO("Simulated Finished");
    return 0;
}
// PERBAIKAN: Menghapus kurung kurawal berlebih