/*
 * Buderus EMS data collector
 *
 * Copyright (C) 2011 Danny Baumann <dannybaumann@web.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __EMSMESSAGE_H__
#define __EMSMESSAGE_H__

#include <vector>
#include <boost/function.hpp>
#include <boost/variant.hpp>

class EmsProto {
    public:
	static const uint8_t addressUBA  = 0x08;
	static const uint8_t addressBC10 = 0x09;
	static const uint8_t addressPC   = 0x0b;
	static const uint8_t addressRC3x = 0x10;
	static const uint8_t addressWM10 = 0x11;
	static const uint8_t addressRC2xStandalone = 0x17;
	static const uint8_t addressRC2xHK1 = 0x18;
	static const uint8_t addressRC2xHK2 = 0x19;
	static const uint8_t addressRC2xHK3 = 0x1a;
	static const uint8_t addressRC2xHK4 = 0x1b;
	static const uint8_t addressMM10HK1 = 0x20;
	static const uint8_t addressMM10HK2 = 0x21;
	static const uint8_t addressMM10HK3 = 0x22;
	static const uint8_t addressMM10HK4 = 0x23;
	static const uint8_t addressSM10 = 0x30;

    public:
#pragma pack(push,1)
	typedef struct {
	    uint8_t year : 7;
	    uint8_t valid : 1;
	    uint8_t month;
	    uint8_t hour;
	    uint8_t day;
	    uint8_t minute;
	} DateTimeRecord;

	typedef struct {
	    uint8_t day;
	    uint8_t month;
	    uint8_t year;
	} DateRecord;

	typedef struct {
	    DateTimeRecord common;
	    uint8_t second;
	    uint8_t dayOfWeek;
	    uint8_t reserved1 : 3;
	    uint8_t running : 1;
	    uint8_t reserved2 : 2;
	    uint8_t dcf : 1;
	    uint8_t dst : 1;
	} SystemTimeRecord;

	typedef struct {
	    uint8_t errorAscii[2];
	    uint16_t code_be16;
	    DateTimeRecord time;
	    uint16_t durationMinutes_be16;
	    uint8_t source;
	} ErrorRecord;

	typedef struct {
	    uint8_t on : 4;
	    uint8_t day : 4;
	    uint8_t time;
	} ScheduleEntry;

	typedef struct {
	    uint8_t day;
	    uint8_t month;
	    uint8_t year;
	} HolidayEntry;
#pragma pack(pop)

	static const uint8_t WWSystemNone = 0;
	static const uint8_t WWSystemDurchlauf = 1;
	static const uint8_t WWSystemKlein = 2;
	static const uint8_t WWSystemGross = 3;
	static const uint8_t WWSystemSpeicherlade = 4;
};

class EmsValue {
    public:
	enum Type {
	    /* numeric */
	    SollTemp, /* HKx, Kessel */
	    IstTemp, /* HKx, Kessel, Waermetauscher, Ruecklauf, WW, Aussen */
	    SetTemp, /* Kessel */
	    MinTemp, /* HKx, Aussentemp. der Region */
	    MaxTemp, /* HKx, WW */
	    TagTemp,
	    NachtTemp,
	    UrlaubTemp,
	    RaumSollTemp,
	    RaumIstTemp,
	    RaumEinfluss,
	    RaumOffset,
	    GedaempfteTemp, /* Aussen */
	    DesinfektionsTemp,
	    RaumTemperaturAenderung,
	    Mischersteuerung,
	    Flammenstrom,
	    Systemdruck,
	    IstModulation,
	    MinModulation,
	    MaxModulation,
	    SollModulation,
	    SollLeistung,
	    EinschaltHysterese,
	    AusschaltHysterese,
	    SchwelleSommerWinter,
	    FrostSchutzTemp,
	    AuslegungsTemp,
	    RaumUebersteuerTemp,
	    AbsenkungsSchwellenTemp,
	    UrlaubAbsenkungsSchwellenTemp,
	    AbsenkungsAbbruchTemp,
	    DurchflussMenge,
	    /* integer */
	    BetriebsZeit,
	    BetriebsZeit2,
	    HeizZeit,
	    WarmwasserbereitungsZeit,
	    Brennerstarts,
	    WarmwasserBereitungen,
	    DesinfektionStunde,
	    HektoStundenVorWartung,
	    EinschaltoptimierungsZeit,
	    AusschaltoptimierungsZeit,
	    AntipendelZeit,
	    NachlaufZeit,
	    PartyZeit,
	    PausenZeit,
	    /* boolean */
	    FlammeAktiv,
	    BrennerAktiv,
	    ZuendungAktiv,
	    PumpeAktiv,
	    ZirkulationAktiv,
	    DreiWegeVentilAufWW,
	    EinmalLadungAktiv,
	    DesinfektionAktiv,
	    NachladungAktiv,
	    WarmwasserBereitung,
	    WarmwasserTempOK,
	    Tagbetrieb,
	    Sommerbetrieb,
	    Ausschaltoptimierung,
	    Einschaltoptimierung,
	    Estrichtrocknung,
	    WWVorrang,
	    Ferien,
	    Urlaub,
	    Party,
	    Pause,
	    Frostschutzbetrieb,
	    SchaltuhrEin,
	    KesselSchalter,
	    EigenesProgrammAktiv,
	    Desinfektion,
	    EinmalLadungsLED,
	    ATDaempfung,
	    SchaltzeitOptimierung,
	    Fuehler1Defekt,
	    Fuehler2Defekt,
	    Stoerung,
	    StoerungDesinfektion,
	    Ladevorgang,
	    /* enum */
	    WWSystemType,
	    Schaltpunkte,
	    Wartungsmeldungen,
	    WartungFaellig,
	    Betriebsart,
	    DesinfektionTag,
	    GebaeudeArt,
	    AbsenkModus,
	    HeizSystem,
	    FuehrungsGroesse,
	    UrlaubAbsenkungsArt,
	    Frostschutz,
	    FBTyp,
	    /* kennlinie */
	    HKKennlinie,
	    /* error */
	    Fehler,
	    /* systemtime */
	    SystemZeit,
	    /* date */
	    Wartungstermin,
	    /* state */
	    ServiceCode,
	    FehlerCode,
	};

	enum SubType {
	    None,
	    HK1,
	    HK2,
	    HK3,
	    HK4,
	    Brenner,
	    Kessel,
	    KesselPumpe,
	    RC,
	    Ruecklauf,
	    Waermetauscher,
	    WW,
	    Zirkulation,
	    Aussen,
	    Abgas,
	    Ansaugluft,
	    Solar,
	    SolarPumpe,
	    SolarSpeicher,
	    SolarKollektor
	};

	enum ReadingType {
	    Numeric,
	    Integer,
	    Boolean,
	    Enumeration,
	    Kennlinie,
	    Error,
	    Date,
	    SystemTime,
	    Formatted
	};

	struct ErrorEntry {
	    uint8_t type;
	    unsigned int index;
	    EmsProto::ErrorRecord record;
	};

	typedef boost::variant<
	    float, // numeric
	    unsigned int, // integer
	    bool, // boolean
	    uint8_t, // enumeration
	    std::vector<uint8_t>, // kennlinie
	    ErrorEntry, // error
	    EmsProto::DateRecord, // date
	    EmsProto::SystemTimeRecord, // systemtime
	    std::string // formatted
	> Reading;

    public:
	EmsValue(Type type, SubType subType, const uint8_t *value, size_t len, int divider,
		 bool isSigned, const std::vector<const uint8_t *> *invalidValues = NULL);
	EmsValue(Type type, SubType subType, uint8_t value, uint8_t bit);
	EmsValue(Type type, SubType subType, uint8_t value);
	EmsValue(Type type, SubType subType, uint8_t low, uint8_t medium, uint8_t high);
	EmsValue(Type type, SubType subType, const ErrorEntry& error);
	EmsValue(Type type, SubType subType, const EmsProto::DateRecord& date);
	EmsValue(Type type, SubType subType, const EmsProto::SystemTimeRecord& time);
	EmsValue(Type type, SubType subType, const std::string& value);

	Type getType() const {
	    return m_type;
	}
	SubType getSubType() const {
	    return m_subType;
	}
	ReadingType getReadingType() const {
	    return m_readingType;
	}
	bool isValid() const {
	    return m_isValid;
	}
	template<typename T> const T& getValue() const {
	    return boost::get<T>(m_value);
	}

	// convenience shortcut
	bool isForHK() const {
	    return m_subType == HK1 || m_subType == HK2 || m_subType == HK3 || m_subType == HK4;
	}

    private:
	Type m_type;
	SubType m_subType;
	ReadingType m_readingType;
	Reading m_value;
	bool m_isValid;
};

class EmsMessage
{
    public:
	typedef boost::function<void (const EmsValue& value)> ValueHandler;
	typedef boost::function<const EmsValue * (EmsValue::Type type, EmsValue::SubType subtype)> CacheAccessor;

	EmsMessage(ValueHandler& valueHandler, CacheAccessor cacheAccesor, const std::vector<uint8_t>& data);
	EmsMessage(uint8_t dest, uint8_t type, uint8_t offset,
		   const std::vector<uint8_t>& data, bool expectResponse);

	void handle();

    public:
	uint8_t getSource() const {
	    return m_source;
	}
	uint8_t getDestination() const {
	    return m_dest & 0x7f;
	}
	uint8_t getType() const {
	    return m_type;
	}
	uint8_t getOffset() const {
	    return m_offset;
	}
	const std::vector<uint8_t>& getData() const {
	    return m_data;
	}
	std::vector<uint8_t> getSendData(bool omitSenderAddress) const;

    private:
	void parseUBATotalUptimeMessage();
	void parseUBAMonitorFastMessage();
	void parseUBAMonitorSlowMessage();
	void parseUBAMonitorWWMessage();
	void parseUBAMaintenanceSettingsMessage();
	void parseUBAMaintenanceStatusMessage();
	void parseUBAParameterWWMessage();
	void parseUBAUnknown1Message();
	void parseUBAErrorMessage();
	void parseUBAParametersMessage();

	void parseRCTimeMessage();
	void parseRCOutdoorTempMessage();
	void parseRCSystemParameterMessage();
	void parseRCWWOpmodeMessage();
	void parseRCHKMonitorMessage(EmsValue::SubType subType);
	void parseRCHKOpmodeMessage(EmsValue::SubType subType);
	void parseRCHKScheduleMessage(EmsValue::SubType subType);
	void parseRC20StatusMessage(EmsValue::SubType subType);

	void parseWMTemp1Message();
	void parseWMTemp2Message();

	void parseMMTempMessage(EmsValue::SubType subType);

	void parseSolarMonitorMessage();

    private:
	void parseNumeric(size_t offset, size_t size, int divider,
			  EmsValue::Type type, EmsValue::SubType subtype, bool isSigned = true,
			  const std::vector<const uint8_t *> *invalidValues = NULL);
	void parseInteger(size_t offset, size_t size,
			  EmsValue::Type type, EmsValue::SubType subtype) {
	    parseNumeric(offset, size, 0, type, subtype, false);
	}
	void parseTemperature(size_t offset, EmsValue::Type type, EmsValue::SubType subtype) {
	    parseNumeric(offset, 2, 10, type, subtype, true, &INVALID_TEMPERATURE_VALUES);
	}
	void parseBool(size_t offset, uint8_t bit,
		       EmsValue::Type type, EmsValue::SubType subtype);
	void parseEnum(size_t offset,
		       EmsValue::Type type, EmsValue::SubType subtype);

	bool canAccess(size_t offset, size_t size) {
	    return offset >= m_offset && offset + size <= m_offset + m_data.size();
	}
	EmsValue::SubType determineHKFromAddress(uint8_t address) {
	    if (address == EmsProto::addressRC2xHK2 || address == EmsProto::addressMM10HK2) {
		return EmsValue::HK2;
	    }
	    if (address == EmsProto::addressRC2xHK3 || address == EmsProto::addressMM10HK4) {
		return EmsValue::HK3;
	    }
	    if (address == EmsProto::addressRC2xHK4 || address == EmsProto::addressMM10HK4) {
		return EmsValue::HK4;
	    }
	    return EmsValue::HK1;
	}

    private:
	static const std::vector<const uint8_t *> INVALID_TEMPERATURE_VALUES;
	ValueHandler m_valueHandler;
	CacheAccessor m_cacheAccessor;
	std::vector<unsigned char> m_data;
	uint8_t m_source;
	uint8_t m_dest;
	uint8_t m_type;
	uint8_t m_offset;
};

#endif /* __EMSMESSAGE_H__ */
