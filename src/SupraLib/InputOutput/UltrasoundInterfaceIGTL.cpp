// ================================================================================================
// 
// If not explicitly stated: Copyright (C) 2016-2017, all rights reserved,
//      Rüdiger Göbl
//		Email r.goebl@tum.de
//      Chair for Computer Aided Medical Procedures
//      Technische Universität München
//      Boltzmannstr. 3, 85748 Garching b. München, Germany
//
// ================================================================================================


#include "UltrasoundInterfaceIGTL.h"
#include "USImage.h"

#include <igtlImageMessage.h>
#include <igtlClientSocket.h>
#include <utilities/utility.h>
#include <thread>
#include <chrono>
#include <cmath>

using namespace std;
using namespace std::chrono;

namespace supra
{
	UltrasoundInterfaceIGTL::UltrasoundInterfaceIGTL(tbb::flow::graph & graph, const std::string & nodeID)
		: AbstractInput(graph, nodeID,1)
		, m_connected(false)
		, m_frozen(false)
	{
		m_valueRangeDictionary.set<double>("reconnectInterval", 0.01, 3600, 0.1, "Reconnect Interval [s]");
		m_valueRangeDictionary.set<string>("hostname", "", "Server hostname");
		m_valueRangeDictionary.set<uint32_t>("port", 1, 65535, 18944, "Server port");
		configurationChanged();
	}
	void UltrasoundInterfaceIGTL::initializeDevice()
	{
		//try to connect already here, so we are directly good to go!
		lock_guard<mutex> lock(m_objectMutex);
		m_socket = igtl::ClientSocket::New();
		connectToSever();
	}

	void UltrasoundInterfaceIGTL::freeze()
	{
		m_frozen = true;
	}

	void UltrasoundInterfaceIGTL::unfreeze()
	{
		m_frozen = false;
	}

	void UltrasoundInterfaceIGTL::startAcquisition()
	{
		m_callFrequency.setName("TrIGTL");
		logging::log_info("Starting acquisition...");
		while (getRunning())
		{
			if (!m_connected)
			{
				logging::log_info("Not connected!");
				lock_guard<mutex> lock(m_objectMutex);

				connectToSever();
			}

			//------------------------------------------------------------
			// Wait for a reply
			if (m_connected)
			{
				igtl::MessageHeader::Pointer headerMsg = igtl::MessageHeader::New();
				headerMsg->InitPack();
				int rs = m_socket->Receive(headerMsg->GetPackPointer(), headerMsg->GetPackSize());
				{
					lock_guard<mutex> lock(m_objectMutex);
					if (rs == 0)
					{
						logging::log_warn("UltrasoundInterfaceIGTL: Connection closed.");
						closeSocket();
						continue;
					}
					if (rs != headerMsg->GetPackSize())
					{
						logging::log_warn("UltrasoundInterfaceIGTL: Message size information and actual data size don't match.");
						closeSocket();
						continue;
					}

					if (!m_frozen)
					{
						headerMsg->Unpack();
						if (strcmp(headerMsg->GetDeviceName(), "IntersonRF") == 0) {
							receiveImageData(headerMsg);
						}
						else
						{
							logging::log_info("Header not OK ", headerMsg->GetDeviceName());
							m_socket->Skip(headerMsg->GetBodySizeToRead(), 0);
						}
					}
				}
			}
			else {
				logging::log_warn("UltrasoundInterfaceIGTL: Could not reconnect to the server '", m_hostname, ":", m_port, "'. Retrying in ", m_reconnectInterval, "s.");
				duration<long, std::milli> sleepDuration = milliseconds((long long)round(m_reconnectInterval*1e3));
				this_thread::sleep_for(sleepDuration);
			}
		}

		{
			lock_guard<mutex> lock(m_objectMutex);
			closeSocket();
		}

	}

	void UltrasoundInterfaceIGTL::connectToSever()
	{
		if (!m_connected)
		{
			int r = m_socket->ConnectToServer(m_hostname.c_str(), m_port);

			if (r != 0)
			{
				m_connected = false;
				logging::log_warn("UltrasoundInterfaceIGTL: Could not reconnect to the server '", m_hostname, ":", m_port, "'");
			}
			else {
				m_connected = true;
				logging::log_info("UltrasoundInterfaceIGTL: Connected to the server '", m_hostname, ":", m_port, "'");
			}
		} else {
			logging::log_info("UltrasoundInterfaceIGTL: Already connected to the server");
		}

	}

	void UltrasoundInterfaceIGTL::closeSocket()
	{
		m_connected = false;
		logging::log_warn("UltrasoundInterfaceIGTL: Closing socket to the server '", m_hostname, ":", m_port, "'");
		m_socket->CloseSocket();
	}

	int UltrasoundInterfaceIGTL::receiveImageData(igtl::MessageHeader::Pointer & header)
	{
		igtl::ImageMessage::Pointer imageData = igtl::ImageMessage::New();

		imageData->SetMessageHeader(header);
		imageData->AllocatePack();

		m_socket->Receive(imageData->GetPackBodyPointer(), imageData->GetPackBodySize());

		int c = imageData->Unpack(1);

		if (c & igtl::MessageHeader::UNPACK_BODY)
		{
			igtl::TimeStamp::Pointer ts = igtl::TimeStamp::New();
			imageData->GetTimeStamp(ts);
			int dimms[3];
			imageData->GetDimensions(dimms); // 2048 127 1
			auto pData = make_shared<Container<int16_t>>(
				LocationHost,
				ContainerFactory::getNextStream(),
				imageData->GetImageSize()
			);
			auto props = make_shared<USImageProperties>(
				vec2s{dimms[1], 1},
				dimms[0],
				USImageProperties::ImageType::BMode,
				USImageProperties::ImageState::RF,
				USImageProperties::TransducerType::Linear,
				52.5397
			);
			memcpy(pData->get(), imageData->GetScalarPointer(), imageData->GetImageSize());
			auto pImage = make_shared<USImage>(
				vec2s{dimms[0], dimms[1]},
				pData,
				props,
				ts->GetTimeStamp(),
				ts->GetTimeStamp()
			);
			addData<0>(pImage);
			return 1;
		} else {
			logging::log_error("UltrasoundInterfaceIGTL: Unable to unpack image data!");
			return 0;
		}
	}

	void UltrasoundInterfaceIGTL::configurationEntryChanged(const std::string & configKey)
	{
		std::lock_guard<std::mutex> lock(m_objectMutex);

		if (configKey == "reconnectInterval")
		{
			m_reconnectInterval = m_configurationDictionary.get<double>("reconnectInterval");
		}

		bool reconnectNeccessary = false;
		if (configKey == "hostname")
		{
			m_hostname = m_configurationDictionary.get<string>("hostname");
			reconnectNeccessary = true;
		}
		if (configKey == "port")
		{
			m_port = m_configurationDictionary.get<int>("port");
			reconnectNeccessary = true;
		}

		if (reconnectNeccessary)
		{
			closeSocket();
		}
	}

	void UltrasoundInterfaceIGTL::configurationChanged()
	{
		//read conf values
		m_reconnectInterval = m_configurationDictionary.get<double>("reconnectInterval");

		m_hostname = m_configurationDictionary.get<string>("hostname");
		m_port = m_configurationDictionary.get<uint32_t>("port");
	}
}
