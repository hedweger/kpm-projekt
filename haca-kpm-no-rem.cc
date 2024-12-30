#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("kpm-simul");

int
main(int argc, char* argv[])
{
    std::string direction = "UL";
    std::string mode = "COVERAGE_AREA";
    uint32_t udpPacketSizeBrowsing = 65000; // bytes
    uint32_t udpPacketSizeVideo = 65000;    // bytes
    uint32_t lambdaBrowsing = 10000;     // packets per sec
    uint32_t lambdaVideo = 10000;        // packets per sec
    double totalTxPower = 35.0;          // dBm

    CommandLine cmd(__FILE__);
    cmd.AddValue("direction", "DL|UL", direction);
    cmd.AddValue("mode", "BEAM_SHAPE|COVERAGE_AREA|UE_COVERAGE", mode);
    cmd.AddValue("udpPacketSizeBrowsing", "int bytes", udpPacketSizeBrowsing);
    cmd.AddValue("udpPacketSizeVideo", "int bytes", udpPacketSizeVideo);
    cmd.AddValue("lambdaBrowsing", "int packets/sec", lambdaBrowsing);
    cmd.AddValue("lambdaVideo", "int packets/sec", lambdaVideo);
    cmd.AddValue("power", "int dBm", totalTxPower);

    // If --PrintHelp is provided, display the help message and exit
    cmd.Parse(argc, argv);
    // Scenario parameters (that we will use inside this script):
    uint16_t numGnb = 2;
    uint16_t numUePerGnb = 3;
    uint32_t totalUesVid = 2;    // Total voice UEs
    uint32_t totalUesBrowse = 3; // Total browsing UEs

    int logging = 1;

    // Simulation parameters.
    Time simTime = MilliSeconds(1000);
    Time udpAppStartTime = MilliSeconds(10);

    // Two separate BWPs
    // Video stream
    uint16_t numerologyBwp1 = 4;
    double centralFrequencyBand1 = 2.8e9;
    double bandwidthBand1 = 200e7;
    // Web browsing
    uint16_t numerologyBwp2 = 2;
    double centralFrequencyBand2 = 2.82e9;
    double bandwidthBand2 = 200e7;

    // Where we will store the output files.
    std::string simTag = "default_" + std::to_string(totalTxPower) + "_" + std::to_string(udpPacketSizeBrowsing) + "_" + std::to_string(udpPacketSizeVideo);
    std::string outputDir = "./kpm-out/";

    // Rem parameters
    double xMin = -40.0;
    double xMax = 80.0;
    uint16_t xRes = 50;
    double yMin = -70.0;
    double yMax = 50.0;
    uint16_t yRes = 50;
    double z = 1.5;

    /*
     * Ensure that the frequency band is in the mmWave range
     * and the number of UEs matches the assignment (section 2.2, 2.3).
     */
    NS_ABORT_IF(centralFrequencyBand1 == centralFrequencyBand2);
    NS_ABORT_IF(centralFrequencyBand1 < 2e9 && centralFrequencyBand1 > 7e9);
    NS_ABORT_IF(centralFrequencyBand2 < 2e9 && centralFrequencyBand2 > 7e9);
    NS_ABORT_IF(numGnb < 2);
    // NS_ABORT_IF(numTotalUes < 5); " not necessary? - TH

    // Enable logging for the components
    if (logging > 0)
    {
        LogComponentEnable("kpm-simul", LOG_LEVEL_INFO);
    }
    if (logging > 1)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("NrPdcp", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));

    int64_t randomStream = 1;
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(1);
    gridScenario.SetColumns(numGnb);
    // All units below are in meters
    gridScenario.SetHorizontalBsDistance(10.0);
    gridScenario.SetVerticalBsDistance(10.0);
    gridScenario.SetBsHeight(10);
    gridScenario.SetUtHeight(1.5);
    // must be set before BS number
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(numGnb);
    gridScenario.SetUtNumber((totalUesBrowse + totalUesVid) * numGnb);
    gridScenario.SetScenarioHeight(3);
    gridScenario.SetScenarioLength(3);
    randomStream += gridScenario.AssignStreams(randomStream);
    gridScenario.CreateScenario();

    NodeContainer ueBrowsingWebContainer;
    NodeContainer ueVideoContainer;

    // Distribute UEs to containers
    uint32_t ueCountForBrowsing = 0;
    uint32_t ueCountForVideo = 0;

    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); j++)
    {
        Ptr<Node> ue = gridScenario.GetUserTerminals().Get(j);
        if (j % 2 != 0)
        {
            ueVideoContainer.Add(ue);
            ueCountForVideo++;
        }
        else
        {
            ueBrowsingWebContainer.Add(ue);
            ueCountForBrowsing++;
        }
    }

    NS_ABORT_IF(ueVideoContainer.GetN() < 2);
    NS_ABORT_IF(ueBrowsingWebContainer.GetN() < 3);

    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);

    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;

    CcBwpCreator::SimpleOperationBandConf bandConf1(centralFrequencyBand1,
                                                    bandwidthBand1,
                                                    numCcPerBand,
                                                    BandwidthPartInfo::UMi_StreetCanyon);
    CcBwpCreator::SimpleOperationBandConf bandConf2(centralFrequencyBand2,
                                                    bandwidthBand2,
                                                    numCcPerBand,
                                                    BandwidthPartInfo::UMi_StreetCanyon);

    OperationBandInfo band1 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf1);
    OperationBandInfo band2 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf2);

    /*
     * The configured spectrum division is:
     * ------------Band1--------------|--------------Band2-----------------
     * ------------CC1----------------|--------------CC2-------------------
     * ------------BWP1---------------|--------------BWP2------------------
     */

    /*
     * Attributes of ThreeGppChannelModel still cannot be set in our way.
     * TODO: Coordinate with Tommaso
     */
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(0)));
    nrHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));

    nrHelper->InitializeOperationBand(&band1);

    double x =
        pow(10,
            totalTxPower /
                10); // vscode will show error due to incorrect linking for the LSP, it's fine - TH
    double totalBandwidth = bandwidthBand1;

    nrHelper->InitializeOperationBand(&band2);
    totalBandwidth += bandwidthBand2;
    allBwps = CcBwpCreator::GetAllBwps({band1, band2});

    /*
     * allBwps contains all the spectrum configuration needed for the nrHelper.
     *
     * Now, we can setup the attributes. We can have three kind of attributes:
     * (i) parameters that are valid for all the bandwidth parts and applies to
     * all nodes, (ii) parameters that are valid for all the bandwidth parts
     * and applies to some node only, and (iii) parameters that are different for
     * every bandwidth parts. The approach is:
     *
     * - for (i): Configure the attribute through the helper, and then install;
     * - for (ii): Configure the attribute through the helper, and then install
     * for the first set of nodes. Then, change the attribute through the helper,
     * and install again;
     * - for (iii): Install, and then configure the attributes by retrieving
     * the pointer needed, and calling "SetAttribute" on top of such pointer.
     *
     */

    Packet::EnableChecking();
    Packet::EnablePrinting();

    /*
     *  Case (i): Attributes valid for all the nodes
     */
    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));
    uint32_t bwpIdForBrowsing = 0;
    uint32_t bwpIdForCall = 1;

    nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB",
                                                 UintegerValue(bwpIdForBrowsing));
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("GBR_NON_CONV_VIDEO", UintegerValue(bwpIdForCall));

    nrHelper->SetUeBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB",
                                                UintegerValue(bwpIdForBrowsing));
    nrHelper->SetUeBwpManagerAlgorithmAttribute("GBR_NON_CONV_VIDEO", UintegerValue(bwpIdForCall));

    for (uint32_t i = 0; i < ueVideoContainer.GetN(); ++i)
    {
        Ptr<Node> ue = ueVideoContainer.Get(i);
        nrHelper->SetUeBwpManagerAlgorithmAttribute("GBR_NON_CONV_VIDEO",
                                                    UintegerValue(bwpIdForCall));
    }

    for (uint32_t i = 0; i < ueBrowsingWebContainer.GetN(); ++i)
    {
        Ptr<Node> ue = ueBrowsingWebContainer.Get(i);
        nrHelper->SetUeBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB",
                                                    UintegerValue(bwpIdForBrowsing));
    }

    /*
     * Case (ii): Attributes valid for a subset of the nodes
     */

    // DEFAULTS IN THIS CASE

    /*
     * We have configured the attributes we needed. Now, install and get the pointers
     * to the NetDevices, which contains all the NR stack:
     */

    NetDeviceContainer gnbNetDev =
        nrHelper->InstallGnbDevice(gridScenario.GetBaseStations(), allBwps);
    NetDeviceContainer ueBrowsingWebNetDev =
        nrHelper->InstallUeDevice(ueBrowsingWebContainer, allBwps);
    NetDeviceContainer ueVideoStreamNetDev = nrHelper->InstallUeDevice(ueVideoContainer, allBwps);

    randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueBrowsingWebNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueVideoStreamNetDev, randomStream);

    /*
     * Case (iii): Go node for node and change the attributes we have to setup
     * per-node.
     */

    // Set the appropriate bandwith and TxPower for each gNB
    // Iterate through all gNBs in the gnbNetDev container
    for (uint32_t i = 0; i < gnbNetDev.GetN(); ++i)
    {
        // Calculate the TxPower for the current gNB, considering the total bandwidth and other
        // parameters
        double txPower = 10 * log10((bandwidthBand1 / totalBandwidth) * x);

        // Get the first bandwidth part (0)
        nrHelper->GetGnbPhy(gnbNetDev.Get(i), 0)
            ->SetAttribute("Numerology", UintegerValue(numerologyBwp1));
        nrHelper->GetGnbPhy(gnbNetDev.Get(0), 0)->SetAttribute("TxPower", DoubleValue(txPower));

        // Get the second bandwidth part (1)
        nrHelper->GetGnbPhy(gnbNetDev.Get(i), 1)
            ->SetAttribute("Numerology", UintegerValue(numerologyBwp2));
        nrHelper->GetGnbPhy(gnbNetDev.Get(i), 1)->SetTxPower(txPower);
    }

    // When all the configuration is done, explicitly call UpdateConfig ()
    nrHelper->UpdateDeviceConfigs(gnbNetDev);
    nrHelper->UpdateDeviceConfigs(ueBrowsingWebNetDev);
    nrHelper->UpdateDeviceConfigs(ueVideoStreamNetDev);

    Ptr<Node> pgw = nrEpcHelper->GetPgwNode();
    // Create a remote host to simulate an external network (internet)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute(
        "DataRate",
        DataRateValue(DataRate("100Gb/s"))); // High data rate between PGW and remote host
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500)); // Maximum Transmission Unit (MTU) set
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.000))); // Minimal delay
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Set up IPv4 address for the internet devices and configure routing
    Ipv4AddressHelper ipv4h;
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    ipv4h.SetBase("8.0.0.0", "255.0.0.0"); // IP address range for the internet connection
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Configure routing for the remote host, simulating a route to the mobile UE's network
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
                                               Ipv4Mask("255.0.0.0"),
                                               1);
    internet.Install(gridScenario.GetUserTerminals());
    Ipv4InterfaceContainer ueLowLatIpIface =
        nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueBrowsingWebNetDev));
    Ipv4InterfaceContainer ueVideoIpIface =
        nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueVideoStreamNetDev));

    for (uint32_t i = 0; i < ueLowLatIpIface.GetN(); ++i)
    {
        Ptr<NetDevice> ueDev = ueBrowsingWebNetDev.Get(i);
        Ipv4Address ipAddr = ueLowLatIpIface.GetAddress(i);
    }

    for (uint32_t i = 0; i < ueVideoIpIface.GetN(); ++i)
    {
        Ptr<NetDevice> ueDev = ueVideoStreamNetDev.Get(i);
        Ipv4Address ipAddr = ueVideoIpIface.GetAddress(i);
    }

    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(
            gridScenario.GetUserTerminals().Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    uint32_t callIndex = 0;
    uint32_t browseIndex = 0;

    for (uint32_t i = 0; i < gnbNetDev.GetN(); i++)
    {
        Ptr<NetDevice> bs = gnbNetDev.Get(i);
        for (uint32_t j = 0; j < numUePerGnb; j++)
        {
            Ptr<NetDevice> ueDev;
            if (j % 2 != 0)
            {
                if (callIndex > totalUesVid)
                {
                    NS_LOG_ERROR("UE with ID "
                                 << ueVideoStreamNetDev.Get(callIndex)->GetNode()->GetId()
                                 << "exceeded voice UE limit");
                    continue;
                }
                ueDev = ueVideoStreamNetDev.Get(callIndex++);
            }
            else
            {
                ueDev = ueBrowsingWebNetDev.Get(browseIndex++);
            }

            nrHelper->AttachToGnb(ueDev, bs);
        }
    }

    uint16_t dlPortBrowsing = 1234;
    uint16_t dlPortViedoCall = 1235;

    ApplicationContainer serverApps;

    UdpServerHelper dlPacketSinkBrowsing(dlPortBrowsing);
    UdpServerHelper dlPacketSinkVoiceCall(dlPortViedoCall);
    serverApps.Add(dlPacketSinkBrowsing.Install(ueBrowsingWebContainer));
    serverApps.Add(dlPacketSinkVoiceCall.Install(ueVideoContainer));
    UdpClientHelper dlClientBrowsing;
    dlClientBrowsing.SetAttribute("RemotePort", UintegerValue(dlPortBrowsing));
    dlClientBrowsing.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClientBrowsing.SetAttribute("PacketSize", UintegerValue(udpPacketSizeBrowsing));
    dlClientBrowsing.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaBrowsing)));
    NrEpsBearer bearerBrowsing(NrEpsBearer::NGBR_LOW_LAT_EMBB);
    Ptr<NrEpcTft> tftBrowsing = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfLowLat;
    dlpfLowLat.localPortStart = dlPortBrowsing;
    dlpfLowLat.localPortEnd = dlPortBrowsing;
    tftBrowsing->Add(dlpfLowLat);
    UdpClientHelper dlClientVoice;
    dlClientVoice.SetAttribute("RemotePort", UintegerValue(dlPortViedoCall));
    dlClientVoice.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClientVoice.SetAttribute("PacketSize", UintegerValue(udpPacketSizeVideo));
    dlClientVoice.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaVideo)));

    NrEpsBearer bearerViedo(NrEpsBearer::GBR_CONV_VIDEO);

    Ptr<NrEpcTft> tftVideo = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfViedo;
    dlpfViedo.localPortStart = dlPortViedoCall;
    dlpfViedo.localPortEnd = dlPortViedoCall;
    tftVideo->Add(dlpfViedo);

    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < ueBrowsingWebContainer.GetN(); ++i)
    {
        Ptr<Node> ue = ueBrowsingWebContainer.Get(i);
        Ptr<NetDevice> ueDevice = ueBrowsingWebNetDev.Get(i);
        Address ueAddress = ueLowLatIpIface.GetAddress(i);
        dlClientBrowsing.SetAttribute("RemoteAddress", AddressValue(ueAddress));
        clientApps.Add(dlClientBrowsing.Install(remoteHost));
        nrHelper->ActivateDedicatedEpsBearer(ueDevice, bearerBrowsing, tftBrowsing);
    }

    for (uint32_t i = 0; i < ueVideoContainer.GetN(); ++i)
    {
        Ptr<Node> ue = ueVideoContainer.Get(i);
        Ptr<NetDevice> ueDevice = ueVideoStreamNetDev.Get(i);
        Address ueAddress = ueVideoIpIface.GetAddress(i);
        dlClientVoice.SetAttribute("RemoteAddress", AddressValue(ueAddress));
        clientApps.Add(dlClientVoice.Install(remoteHost));
        nrHelper->ActivateDedicatedEpsBearer(ueDevice, bearerViedo, tftVideo);
    }

    serverApps.Start(udpAppStartTime);
    clientApps.Start(udpAppStartTime);
    serverApps.Stop(simTime);
    clientApps.Stop(simTime);

    nrHelper->EnableTraces();

    FlowMonitorHelper flowmonHelper;
    flowmonHelper.InstallAll();
    NodeContainer endpointNodes;
    endpointNodes.Add(gridScenario.GetUserTerminals());

    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    // Ptr<NrRadioEnvironmentMapHelper> remHelper = CreateObject<NrRadioEnvironmentMapHelper>();
    // remHelper->SetMinX(xMin);
    // remHelper->SetMaxX(xMax);
    // remHelper->SetResX(xRes);
    // remHelper->SetMinY(yMin);
    // remHelper->SetMaxY(yMax);
    // remHelper->SetResY(yRes);
    // remHelper->SetZ(z);
    // std::string remSimTag = direction + "_" + mode;
    // remHelper->SetSimTag(remSimTag);

    // uint16_t remBwpId = 0;

    // for (uint32_t i = 0; i < gnbNetDev.GetN(); i++)
    // {
    //     Ptr<NetDevice> bs = gnbNetDev.Get(i); // Get the base station device

    //     // Identify the first UE attached to the base station
    //     Ptr<NetDevice> firstUeNetNode;
    //     bool ueAssigned = false;

    //     // Loop through the UEs attached to the base station
    //     for (uint32_t j = 0; j < numUePerGnb; j++)
    //     {
    //         Ptr<NetDevice> ueDev;
    //         if (j % 2 == 0 && callIndex > 0)
    //         {                                                   // Check if voice UE is available
    //             ueDev = ueVideoStreamNetDev.Get(callIndex - 1); // First voice UE
    //             firstUeNetNode = ueDev;
    //             ueAssigned = true;
    //             break; // We found the first UE, break the loop
    //         }
    //         else if (browseIndex > 0)
    //         { // Check if browsing UE is available
    //             ueDev = ueBrowsingWebNetDev.Get(browseIndex - 1); // First browsing UE
    //             firstUeNetNode = ueDev;
    //             ueAssigned = true;
    //             break; // We found the first UE, break the loop
    //         }
    //     }

    //     // If a UE was assigned, set beamforming vector for that UE
    //     if (ueAssigned)
    //     {
    //         gnbNetDev.Get(i)
    //             ->GetObject<NrGnbNetDevice>()
    //             ->GetPhy(remBwpId)
    //             ->GetSpectrumPhy()
    //             ->GetBeamManager()
    //             ->ChangeBeamformingVector(firstUeNetNode);
    //         NS_LOG_INFO("Setting beamforming for UE with ID " << firstUeNetNode->GetNode()->GetId()
    //                                                           << " attached to BS with ID "
    //                                                           << bs->GetNode()->GetId());
    //     }
    //     else
    //     {
    //         NS_LOG_INFO("Beamforming for UE with ID " << firstUeNetNode->GetNode()->GetId()
    //                                                   << " not set (not attached to a bs)");
    //     }
    // }

    // if (direction == "DL")
    // {
    //     if (mode == "BEAM_SHAPE")
    //     {
    //         remHelper->SetRemMode(NrRadioEnvironmentMapHelper::BEAM_SHAPE);
    //         remHelper->CreateRem(gnbNetDev, ueVideoStreamNetDev.Get(0), remBwpId); // DlRem
    //     }
    //     else if (mode == "COVERAGE_AREA")
    //     {
    //         remHelper->SetRemMode(NrRadioEnvironmentMapHelper::COVERAGE_AREA);
    //         remHelper->CreateRem(gnbNetDev, ueVideoStreamNetDev.Get(0), remBwpId); // DlRem
    //     }
    //     else if (mode == "UE_COVERAGE")
    //     {
    //         remHelper->SetRemMode(NrRadioEnvironmentMapHelper::UE_COVERAGE);
    //         remHelper->CreateRem(gnbNetDev, ueBrowsingWebNetDev.Get(0), remBwpId); // DlRem
    //     }
    //     else
    //     {
    //         NS_LOG_ERROR("Invalid mode for DL REM: " << mode);
    //     }
    // }
    // else if (direction == "UL")
    // {
    //     if (mode == "BEAM_SHAPE")
    //     {
    //         remHelper->SetRemMode(NrRadioEnvironmentMapHelper::BEAM_SHAPE);
    //         remHelper->CreateRem(ueVideoStreamNetDev, gnbNetDev.Get(0), remBwpId); // UlRem
    //     }
    //     else if (mode == "COVERAGE_AREA")
    //     {
    //         remHelper->SetRemMode(NrRadioEnvironmentMapHelper::COVERAGE_AREA);
    //         remHelper->CreateRem(ueVideoStreamNetDev, gnbNetDev.Get(0), remBwpId); // UlRem
    //     }
    //     else if (mode == "UE_COVERAGE")
    //     {
    //         remHelper->SetRemMode(NrRadioEnvironmentMapHelper::UE_COVERAGE);
    //         remHelper->CreateRem(ueBrowsingWebNetDev, gnbNetDev.Get(0), remBwpId); // UlRem
    //     }
    //     else
    //     {
    //         NS_LOG_ERROR("Invalid mode for UL REM: " << mode);
    //     }
    // }
    // else
    // {
    //     NS_LOG_ERROR("Invalid direction for REM: " << direction);
    // }

    Simulator::Stop(simTime);
    NS_LOG_INFO("Starting the simulation ...");
    Simulator::Run();
    NS_LOG_INFO("Simulation finished ...");

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;

    std::ofstream outFile;
    std::string filename = outputDir + "/" + simTag;
    outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!outFile.is_open())
    {
        std::cerr << "Can't open file " << filename << std::endl;
        return 1;
    }

    outFile.setf(std::ios_base::fixed);

    double flowDuration = (simTime - udpAppStartTime).GetSeconds();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::stringstream protoStream;
        protoStream << (uint16_t)t.protocol;
        if (t.protocol == 6)
        {
            protoStream.str("TCP");
        }
        if (t.protocol == 17)
        {
            protoStream.str("UDP");
        }
        outFile << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                << t.destinationAddress << ":" << t.destinationPort << ") proto "
                << protoStream.str() << "\n";
        outFile << "  Tx Packets: " << i->second.txPackets << "\n";
        outFile << "  Tx Bytes:   " << i->second.txBytes << "\n";
        outFile << "  TxOffered:  " << i->second.txBytes * 8.0 / flowDuration / 1000.0 / 1000.0
                << " Mbps\n";
        outFile << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        outFile << "  Lost Packets: " << i->second.txPackets - i->second.rxPackets << "\n";
        outFile << "  Packet loss: "
                << (((i->second.txPackets - i->second.rxPackets) * 1.0) / i->second.txPackets) * 100
                << "%" << "\n";

        if (i->second.rxPackets > 0)
        {
            // Measure the duration of the flow from receiver's perspective
            averageFlowThroughput += i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000;
            averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;

            outFile << "  Throughput: " << i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000
                    << " Mbps\n";
            outFile << "  Mean delay:  "
                    << 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets << " ms\n";
            // outFile << "  Mean upt:  " << i->second.uptSum / i->second.rxPackets / 1000/1000 << "
            // Mbps \n";
            outFile << "  Mean jitter:  "
                    << 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets << " ms\n";
        }
        else
        {
            outFile << "  Throughput:  0 Mbps\n";
            outFile << "  Mean delay:  0 ms\n";
            outFile << "  Mean jitter: 0 ms\n";
        }
        outFile << "  Rx Packets: " << i->second.rxPackets << "\n";
    }

    double meanFlowThroughput = averageFlowThroughput / stats.size();
    double meanFlowDelay = averageFlowDelay / stats.size();

    outFile << "\n\n  Mean flow throughput: " << meanFlowThroughput << "\n";
    outFile << "  Mean flow delay: " << meanFlowDelay << "\n";

    outFile.close();

    std::ifstream f(filename.c_str());

    if (f.is_open())
    {
        std::cout << f.rdbuf();
    }

    Simulator::Destroy();

    if (argc == 0)
    {
        double toleranceMeanFlowThroughput = 0.0001 * 56.258560;
        double toleranceMeanFlowDelay = 0.0001 * 0.553292;

        if (meanFlowThroughput >= 56.258560 - toleranceMeanFlowThroughput &&
            meanFlowThroughput <= 56.258560 + toleranceMeanFlowThroughput &&
            meanFlowDelay >= 0.553292 - toleranceMeanFlowDelay &&
            meanFlowDelay <= 0.553292 + toleranceMeanFlowDelay)
        {
            return EXIT_SUCCESS;
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    else if (argc == 1 and numUePerGnb == 9) // called from examples-to-run.py with these parameters
    {
        double toleranceMeanFlowThroughput = 0.0001 * 47.858536;
        double toleranceMeanFlowDelay = 0.0001 * 10.504189;

        if (meanFlowThroughput >= 47.858536 - toleranceMeanFlowThroughput &&
            meanFlowThroughput <= 47.858536 + toleranceMeanFlowThroughput &&
            meanFlowDelay >= 10.504189 - toleranceMeanFlowDelay &&
            meanFlowDelay <= 10.504189 + toleranceMeanFlowDelay)
        {
            return EXIT_SUCCESS;
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    else
    {
        return EXIT_SUCCESS; // we dont check other parameters configurations at the moment
    }
}
