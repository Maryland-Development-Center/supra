// ================================================================================================
// 
// If not explicitly stated: Copyright (C) 2011-2016, all rights reserved,
//      Christoph Hennersperger 
//		EmaiL christoph.hennersperger@tum.de
//      Chair for Computer Aided Medical Procedures
//      Technische Universität München
//      Boltzmannstr. 3, 85748 Garching b. München, Germany
//	and
//		Rüdiger Göbl
//		Email r.goebl@tum.de
//
// ================================================================================================

#ifndef __ULTRASOUNDINTERFACEIGTL_H__
#define __ULTRASOUNDINTERFACEIGTL_H__

#ifdef HAVE_DEVICE_TRACKING_IGTL

#include <AbstractInput.h>
#include <TrackerDataSet.h>

#include <mutex>

#include <igtlClientSocket.h>
#include <igtlMessageHeader.h>

#include "Beamformer/RxBeamformerParameters.h"


namespace supra
{
	class UltrasoundInterfaceIGTL : public AbstractInput
	{
	public:

		UltrasoundInterfaceIGTL(tbb::flow::graph & graph, const std::string & nodeID);
		~UltrasoundInterfaceIGTL() {};

		//Functions to be overwritten
	public:
		virtual void initializeDevice();
		virtual bool ready() { return true; };

		virtual std::vector<size_t> getImageOutputPorts() { return{ 0 }; };
		virtual std::vector<size_t> getTrackingOutputPorts() { return{}; };

		virtual void freeze();
		virtual void unfreeze();

	protected:
		virtual void startAcquisition();
		//Needs to be thread safe
		virtual void stopAcquisition() {};
		//Needs to be thread safe
		virtual void configurationEntryChanged(const std::string& configKey);
		//Needs to be thread safe
		virtual void configurationChanged();

	private:
		void connectToSever();
		void closeSocket();

		int receiveImageData(igtl::MessageHeader::Pointer & header);

		double m_reconnectInterval;
		std::string m_hostname;
		uint32_t m_port;
		
		std::atomic_bool m_frozen;

		std::mutex m_objectMutex;
		igtl::ClientSocket::Pointer m_socket;
		bool m_connected;

		std::string m_metaDataFilename;
		std::shared_ptr<const RxBeamformerParameters> m_rxparams;
	};
}

#endif //!HAVE_DEVICE_TRACKING_IGTL

#endif //!__ULTRASOUNDINTERFACEIGTL_H__
