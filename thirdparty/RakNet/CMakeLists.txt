find_package (Threads REQUIRED)

set (RAKNET_SOURCES
  Source/Base64Encoder.cpp
  Source/BitStream.cpp
  Source/CCRakNetSlidingWindow.cpp   
  Source/CCRakNetUDT.cpp
  Source/CheckSum.cpp
  Source/CloudClient.cpp
  Source/CloudCommon.cpp
  Source/CloudServer.cpp
  Source/CommandParserInterface.cpp  
  Source/ConnectionGraph2.cpp        
  Source/ConsoleServer.cpp
  Source/DataCompressor.cpp
  Source/DirectoryDeltaTransfer.cpp  
  Source/DR_SHA1.cpp
  Source/DS_BytePool.cpp
  Source/DS_ByteQueue.cpp
  Source/DS_HuffmanEncodingTree.cpp  
  Source/DS_Table.cpp
  Source/DynDNS.cpp
  Source/EmailSender.cpp
  Source/EpochTimeToString.cpp       
  Source/FileList.cpp
  Source/FileListTransfer.cpp        
  Source/FileOperations.cpp
  Source/FormatString.cpp
  Source/FullyConnectedMesh2.cpp     
  Source/Getche.cpp
  Source/Gets.cpp
  Source/GetTime.cpp
  Source/gettimeofday.cpp
  Source/GridSectorizer.cpp
  Source/HTTPConnection.cpp
  Source/HTTPConnection2.cpp
  Source/IncrementalReadInterface.cpp
  Source/Itoa.cpp
  Source/LinuxStrings.cpp
  Source/LocklessTypes.cpp
  Source/LogCommandParser.cpp
  Source/MessageFilter.cpp
  Source/NatPunchthroughClient.cpp
  Source/NatPunchthroughServer.cpp
  Source/NatTypeDetectionClient.cpp
  Source/NatTypeDetectionCommon.cpp
  Source/NatTypeDetectionServer.cpp
  Source/NetworkIDManager.cpp
  Source/NetworkIDObject.cpp
  Source/PacketConsoleLogger.cpp
  Source/PacketFileLogger.cpp
  Source/PacketizedTCP.cpp
  Source/PacketLogger.cpp
  Source/PacketOutputWindowLogger.cpp
  Source/PluginInterface2.cpp
  Source/PS4Includes.cpp
  Source/Rackspace.cpp
  Source/RakMemoryOverride.cpp
  Source/RakNetCommandParser.cpp
  Source/RakNetSocket.cpp
  Source/RakNetSocket2.cpp
  Source/RakNetSocket2_360_720.cpp
  Source/RakNetSocket2_Berkley.cpp
  Source/RakNetSocket2_Berkley_NativeClient.cpp
  Source/RakNetSocket2_NativeClient.cpp
  Source/RakNetSocket2_PS3_PS4.cpp
  Source/RakNetSocket2_PS4.cpp
  Source/RakNetSocket2_Vita.cpp
  Source/RakNetSocket2_WindowsStore8.cpp
  Source/RakNetSocket2_Windows_Linux.cpp
  Source/RakNetSocket2_Windows_Linux_360.cpp
  Source/RakNetStatistics.cpp
  Source/RakNetTransport2.cpp
  Source/RakNetTypes.cpp
  Source/RakPeer.cpp
  Source/RakSleep.cpp
  Source/RakString.cpp
  Source/RakThread.cpp
  Source/RakWString.cpp
  Source/Rand.cpp
  Source/RandSync.cpp
  Source/ReadyEvent.cpp
  Source/RelayPlugin.cpp
  Source/ReliabilityLayer.cpp
  Source/Router2.cpp
  Source/RPC4Plugin.cpp
  Source/SecureHandshake.cpp
  Source/SendToThread.cpp
  Source/SignaledEvent.cpp
  Source/SimpleMutex.cpp
  Source/SocketLayer.cpp
  Source/StatisticsHistory.cpp
  Source/StringCompressor.cpp
  Source/StringTable.cpp
  Source/SuperFastHash.cpp
  Source/TableSerializer.cpp
  Source/TCPInterface.cpp
  Source/TeamBalancer.cpp
  Source/TeamManager.cpp
  Source/TelnetTransport.cpp
  Source/ThreadsafePacketLogger.cpp
  Source/TwoWayAuthentication.cpp
  Source/UDPForwarder.cpp
  Source/UDPProxyClient.cpp
  Source/UDPProxyCoordinator.cpp
  Source/UDPProxyServer.cpp
  Source/VariableDeltaSerializer.cpp
  Source/VariableListDeltaTracker.cpp
  Source/VariadicSQLParser.cpp
  Source/VitaIncludes.cpp
  Source/WSAStartupSingleton.cpp
  Source/_FindFirst.cpp
)

add_library(RakNet STATIC ${RAKNET_SOURCES})
if (WIN32)
  target_link_libraries(RakNet PRIVATE Threads::Threads wsock32 ws2_32 Iphlpapi)
  target_compile_options(RakNet PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/w>"  # Disable all warnings for MSVC
  )
else()
  target_link_libraries(RakNet PRIVATE Threads::Threads)
  target_compile_options(RakNet PRIVATE "-fPIC")
endif()
target_include_directories(RakNet PUBLIC Source)