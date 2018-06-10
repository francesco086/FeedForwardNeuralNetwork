#ifndef FED_NETWORK_UNIT
#define FED_NETWORK_UNIT

#include "NetworkUnit.hpp"
#include "NetworkUnitFeederInterface.hpp"

#include <cstddef> // for NULL

// Network Unit with Feeder
class FedNetworkUnit: virtual public NetworkUnit
{
protected:

    // Feeder of the unit
    // The feeder of a unit is a class that takes care of providing the input to the unit, when called via: _feeder->getFeed()
    NetworkUnitFeederInterface * _feeder;

public:

    // Constructor and destructor
    FedNetworkUnit(NetworkUnitFeederInterface * feeder = NULL){_feeder = feeder;}
    virtual ~FedNetworkUnit(){if (_feeder) delete _feeder; _feeder=NULL;}

    // Setters and getters
    void setFeeder(NetworkUnitFeederInterface * feeder){if (_feeder) delete _feeder; _feeder = feeder;}
    NetworkUnitFeederInterface * getFeeder(){return _feeder;}

    // string code getters
    virtual std::string getIdCode(){return "fnwu";} // return identifier for unit type

    virtual std::string getMemberIdCodes(){return _feeder ? _feeder->getIdCode() : "";} // return feeder IdCodes
    virtual std::string getMemberFullCodes(){return _feeder ? _feeder->getFullCode() : "";} // return feeder IdCodes + Params

    // Computation
    void computeFeed();
    void computeDerivatives();
};


#endif