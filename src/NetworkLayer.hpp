#ifndef NETWORK_LAYER
#define NETWORK_LAYER

#include "BaseComponent.hpp"
#include "NetworkUnit.hpp"
#include "OffsetUnit.hpp"

#include <vector>
#include <string>

class NetworkLayer: virtual public BaseComponent
{
protected:

    OffsetUnit * _U_off;
    std::vector<NetworkUnit *> _U; // this vector stores units of all derived types

public:

    // --- Constructor

    NetworkLayer();
    virtual void construct(const int &nunits) = 0; // should add non-offset units until a total of nunits units of any type are present


    // --- Destructor

    virtual ~NetworkLayer();
    virtual void deconstruct(); // should remove the non-offset units

    // --- Class IdCode

    std::string getClassIdCode(){return "layer";}

    // --- Getters

    int getNUnits(){return _U.size();}
    NetworkUnit * getUnit(const int & i){return _U[i];}
    OffsetUnit * getOffsetUnit(){return _U_off;}


    // --- Modify structure

    void setSize(const int &nunits); // uses deconstruct and then construct to resize the layer


    // --- Variational Parameters

    virtual bool setVariationalParameter(const int &id, const double &vp) {return false;};
    virtual bool getVariationalParameter(const int &id, double &vp) {return false;}
    virtual int getNVariationalParameters() {return 0;};
    virtual int setVariationalParametersID(const int &id_vp) { return id_vp;};


    // --- Values to compute

    void addCrossSecondDerivativeSubstrate(const int &nx0, const int &nvp);
    void addCrossFirstDerivativeSubstrate(const int &nx0, const int &nvp);
    void addVariationalFirstDerivativeSubstrate(const int &nvp);
    void addSecondDerivativeSubstrate(const int &nx0);
    void addFirstDerivativeSubstrate(const int &nx0);


    // --- Computation

    virtual void computeValues();
};

#endif