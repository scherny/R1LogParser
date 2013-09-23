// R1LogParser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <time.h>
#include <chrono>

#include <Poco/Util/PropertyFileConfiguration.h>
#include <Poco/String.h>
#include <Poco/StringTokenizer.h>

std::map<std::string, std::string> g_ApduCmdDesc;
std::map<std::string, std::string> g_ApduRespDesc;

class Trn
{
public:
	Trn(std::string const& str) : m_IsCompleted(false)
	{
		ParseTime(str, m_hBeg, m_mBeg, m_sBeg, m_msBeg);
	}

	void Complete(std::string const& str)
	{
		ParseTime(str, m_hEnd, m_mEnd, m_sEnd, m_msEnd);
		m_IsCompleted = true;
	}

	void Parse(std::string const& s)
	{
		std::string::size_type i = s.find("ICC OUT: ");
		bool is_out_apdu = false, is_in_apdu = false;
		std::string apdu;

		if ( i != std::string::npos )
		{
			apdu = s.substr(i + 9 + 44); // header size is 44
			is_out_apdu = true;
		}
		else if ( (i = s.find("ICC IN: ")) != std::string::npos )
		{
			apdu = s.substr(i + 8 + 12); // header size is 12
			is_in_apdu = true;
		}

		if ( is_out_apdu || is_in_apdu )
		{
			i = apdu.find(" ");
			if ( i != std::string::npos )
			{
				apdu.erase(i);
			}

			if ( is_out_apdu )
			{
				m_ApduCmd = Poco::toUpper(apdu);
				auto t = g_ApduCmdDesc.find(m_ApduCmd.substr(2, 2));
				if ( t != g_ApduCmdDesc.end() )
					m_ApduCmdDescr = t->second;
			}
			else if ( is_in_apdu )
			{
				m_ApduResp = Poco::toUpper(apdu);

				if ( m_ApduResp.size() >= 4 )
				{
					auto t = g_ApduRespDesc.find(m_ApduResp.substr(m_ApduResp.size()-4));
					if ( t != g_ApduRespDesc.end() )
						m_ApduRespDescr = t->second;
					else
					{
						t = g_ApduRespDesc.find(m_ApduResp.substr(m_ApduResp.size() - 4, 2));
						if ( t != g_ApduRespDesc.end() )
							m_ApduRespDescr = t->second;
					}
				}
			}
		}
	}

	bool IsCompleted() const { return m_IsCompleted; }
	std::string const& Apdu() const { return m_ApduCmd; }
	std::string const& ApduDescr() const { return m_ApduCmdDescr; }
	std::string const& ApduResp() const { return m_ApduResp; }
	std::string const& ApduRespDesc() const { return m_ApduRespDescr; }
	std::chrono::milliseconds GetTrnBegEpoch() const { return m_hBeg + m_mBeg + m_sBeg + m_msBeg; }
	std::chrono::milliseconds GetTrnEndEpoch() const { return m_hEnd + m_mEnd + m_sEnd + m_msEnd; }
	long long GetTrnTime() const { return (GetTrnEndEpoch() - GetTrnBegEpoch()).count(); }
	long long GetPrevTrnTime(Trn const& t) { return (GetTrnBegEpoch() - t.GetTrnEndEpoch()).count(); }

protected:
	void ParseTime(std::string const& str, std::chrono::hours& hh, std::chrono::minutes& mm, std::chrono::seconds& ss, std::chrono::milliseconds& mss)
	{
		std::string::size_type i = str.find("]");
		if ( i != std::string::npos )
		{
			std::string tmp = Poco::trimLeft(str.substr(i + 1));
			i = tmp.find(" ");
			if ( i != std::string::npos )
			{
				std::string date_time = tmp.substr(0, i);
				Poco::StringTokenizer tok(date_time, ":");

				hh = std::chrono::hours(std::stoi(tok[0]));
				mm = std::chrono::minutes(std::stoi(tok[1]));
				ss = std::chrono::seconds(std::stoi(tok[2]));
				mss = std::chrono::milliseconds(std::stoi(tok[3]));
			}
		}
	}

private:
	std::chrono::hours m_hBeg;
	std::chrono::minutes m_mBeg;
	std::chrono::seconds m_sBeg;
	std::chrono::milliseconds m_msBeg;

	std::chrono::hours m_hEnd;
	std::chrono::minutes m_mEnd;
	std::chrono::seconds m_sEnd;
	std::chrono::milliseconds m_msEnd;

	bool m_IsCompleted;
	std::string m_ApduCmd;
	std::string m_ApduCmdDescr;
	std::string m_ApduResp;
	std::string m_ApduRespDescr;
};

std::list<Trn> g_Trns;

bool ParseLine(std::string const& buf, std::string const& card_begin_trn, std::string const& card_end_trn)
{
	if ( buf.find(card_begin_trn) != std::string::npos )
	{
		g_Trns.emplace_back(buf);
		return true;
	}
	else if ( !g_Trns.empty() )
	{
		if ( buf.find(card_end_trn) != std::string::npos )
		{
			g_Trns.back().Complete(buf);
			return true;
		}
		else if ( !g_Trns.back().IsCompleted() )
		{
			g_Trns.back().Parse(buf);
		}
	}
	return false;
}

int _tmain(int argc, _TCHAR* argv [])
{
	if ( argc != 2 )
	{
		std::cout << "No log" << std::endl;
		return 1;
	}

	try
	{
		std::ifstream log_file(argv[1]);

		if ( !log_file )
		{
			std::cout << "Can't open log file: " << argv[1] << std::endl;
			return 2;
		}

		std::string card_begin_trn;
		std::string card_end_trn;
		{
			Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> conf;
			conf = new Poco::Util::PropertyFileConfiguration("R1LogParser.properties");

			card_begin_trn = conf->getString("CardTrnBegin");
			card_end_trn = conf->getString("CardTrnEnd");

			std::string card_apdu = conf->getString("APDU_IN");

			Poco::StringTokenizer si(card_apdu, ";");
			for ( auto const& i : si )
			{
				Poco::StringTokenizer ss(i, ":");

				if ( ss.count() != 2 )
					continue;
				g_ApduCmdDesc[Poco::toUpper(Poco::trim(ss[0]))] = Poco::trim(ss[1]);
			}

			std::string card_apdu_resp = conf->getString("APDU_OUT");

			Poco::StringTokenizer so(card_apdu_resp, ";");
			for ( auto const& i : so )
			{
				Poco::StringTokenizer ss(i, ":");

				if ( ss.count() != 2 )
					continue;
				g_ApduRespDesc[Poco::toUpper(Poco::trim(ss[0]))] = Poco::trim(ss[1]);
			}
		}

		char buf[10 * 1024];

		while ( log_file.getline(buf, sizeof(buf) -1) )
		{
			ParseLine(buf, card_begin_trn, card_end_trn);
		}
		std::cout << "Found card trn: " << g_Trns.size() << std::endl;

		if ( !g_Trns.empty() )
		{
			std::chrono::milliseconds total_time(g_Trns.back().GetPrevTrnTime(g_Trns.front()));
			std::chrono::seconds total_time_in_s((total_time/1000).count());
			std::cout << "Total trn time: " << total_time.count() << "ms, or " << total_time_in_s.count() << "s" << std::endl << std::endl;

			std::cout << "Timings: " << std::endl;

			size_t n = 0;
			auto prev = g_Trns.begin();
			for ( auto i = g_Trns.begin(); i != g_Trns.end(); ++i )
			{
				if ( prev == i )
					std::cout << "[" << ++n << "] " << i->GetTrnTime() << "ms " << std::endl;
				else
					std::cout << "[" << ++n << "] " << "->" << i->GetPrevTrnTime(*prev) << "ms, " << i->GetTrnTime() << "ms" << std::endl;
				prev = i;
			}

			std::cout << std::endl << std::endl;
			std::cout << "Trns: " << std::endl;
			n = 0;
			for ( auto i : g_Trns )
			{
				std::cout << "[" << ++n << "] " << i.Apdu() << " {" << i.ApduDescr() << "}, " << i.GetTrnTime() << "ms => " << i.ApduResp() << " {" << i.ApduRespDesc()  << "}" << std::endl;
			}
		}
	}
	catch ( Poco::NotFoundException ex )
	{
		std::cout << "Bad configuration. Missing property: " << ex.message() << std::endl;
	}
	catch ( std::exception ex )
	{
		std::cout << "Unknown exception: " << ex.what() << std::endl;
	}
	return 0;
}
