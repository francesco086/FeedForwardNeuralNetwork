#include "FeedForwardNeuralNetwork.hpp"

#include "NNUnit.hpp"
#include "ActivationFunctionManager.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <random>


// --- Variational Parameters

int FeedForwardNeuralNetwork::getNBeta()
{
   using namespace std;
   int nbeta=0;
   for (vector<NNLayer *>::size_type i=0; i<_L.size(); ++i)
   {
      for (int j=0; j<_L[i]->getNUnits(); ++j)
      {
         if (_L[i]->getUnit(j)->getFeeder())
         {
            nbeta += _L[i]->getUnit(j)->getFeeder()->getNBeta();
         }
      }
   }
   return nbeta;
}


double FeedForwardNeuralNetwork::getBeta(const int &ib)
{
   using namespace std;
   if ( ib >= this->getNBeta() )
   {
      cout << endl << "ERROR FeedForwardNeuralNetwork::getBeta : index out of boundaries" << endl;
      cout << ib << " against the maximum allowed " << this->getNBeta() << endl << endl;
   }
   else
   {
      int idx=0;
      for (vector<NNLayer *>::size_type i=0; i<_L.size(); ++i)
      {
         for (int j=0; j<_L[i]->getNUnits(); ++j)
         {
            if (_L[i]->getUnit(j)->getFeeder())
            {
               for (int k=0; k<_L[i]->getUnit(j)->getFeeder()->getNBeta(); ++k)
               {
                  if (idx==ib) return _L[i]->getUnit(j)->getFeeder()->getBeta(k);
                  idx++;
               }
            }
         }
      }
   }
   cout << endl << "ERROR FeedForwardNeuralNetwork::getBeta : index not found" << endl;
   cout << ib << " against the maximum allowed " << this->getNBeta() << endl << endl;
   return -666.;
}


void FeedForwardNeuralNetwork::setBeta(const int &ib, const double &beta)
{
   using namespace std;
   if ( ib >= this->getNBeta() )
   {
      cout << endl << "ERROR FeedForwardNeuralNetwork::getBeta : index out of boundaries" << endl;
      cout << ib << " against the maximum allowed " << this->getNBeta() << endl << endl;
   }
   else
   {
      int idx=0;
      for (vector<NNLayer *>::size_type i=0; i<_L.size(); ++i)
      {
         for (int j=0; j<_L[i]->getNUnits(); ++j)
         {
            if (_L[i]->getUnit(j)->getFeeder())
            {
               for (int k=0; k<_L[i]->getUnit(j)->getFeeder()->getNBeta(); ++k)
               {
                  if (idx==ib) _L[i]->getUnit(j)->getFeeder()->setBeta(k,beta);
                  idx++;
               }
            }
         }
      }
   }

}


void FeedForwardNeuralNetwork::randomizeBetas()
{
    std::random_device rdev;
    std::mt19937_64 rgen = std::mt19937_64(rdev());
    std::uniform_real_distribution<double> rd = std::uniform_real_distribution<double>(-3.,3.);

    // set betas to new random values
    for (int i=0; i<this->getNBeta(); ++i) this->setBeta(i, rd(rgen));
}



// --- Computation

double FeedForwardNeuralNetwork::getVariationalFirstDerivative(const int &i, const int &iv1d)
{
   return ( _L.back()->getUnit(i+1)->getVariationalFirstDerivativeValue(iv1d) );
}


double FeedForwardNeuralNetwork::getSecondDerivative(const int &i, const int &i2d)
{
   return ( _L.back()->getUnit(i+1)->getSecondDerivativeValue(i2d) );
}


double FeedForwardNeuralNetwork::getFirstDerivative(const int &i, const int &i1d)
{
   return ( _L.back()->getUnit(i+1)->getFirstDerivativeValue(i1d) );
}


double FeedForwardNeuralNetwork::getOutput(const int &i)
{
   return _L.back()->getUnit(i+1)->getValue();
}


void FeedForwardNeuralNetwork::getOutput(double * out)
{
    for (int i=1; i<_L.back()->getNUnits(); ++i){
        out[i-1] = _L.back()->getUnit(i)->getValue();
    }
}


void FeedForwardNeuralNetwork::FFPropagate()
{
   for (std::vector<NNLayer *>::size_type i=0; i<_L.size(); ++i)
   {
      _L[i]->computeValues();
   }
}


void FeedForwardNeuralNetwork::setInput(const double *in)
{
   // set the protovalues of the first layer units
   for (int i=1; i<_L[0]->getNUnits(); ++i)
   {
      _L[0]->getUnit(i)->setProtoValue(in[i-1]);
   }
   // set the first derivatives
   if (_flag_1d)
   {
      for (int i=1; i<_L[0]->getNUnits(); ++i)
      {
         _L[0]->getUnit(i)->setFirstDerivativeValue(   i-1,
              _L[0]->getUnit(i)->getActivationFunction()->f1d( _L[0]->getUnit(i)->getProtoValue() )   );
      }
   }
}


void FeedForwardNeuralNetwork::setInput(const int &i, const double &in)
{
    // set the protovalues of the first layer units
    _L[0]->getUnit(i+1)->setProtoValue(in);
    // set the first derivatives
    if (_flag_1d)
    {
        _L[0]->getUnit(i+1)->setFirstDerivativeValue(i, _L[0]->getUnit(i+1)->getActivationFunction()->f1d( _L[0]->getUnit(i)->getProtoValue() ) );
    }
}



// --- Substrates

void FeedForwardNeuralNetwork::addLastHiddenLayerVariationalFirstDerivativeSubstrate()
{
   // count the total number of variational parameters
   _nvp=0;
   for (std::vector<NNLayer *>::size_type i=_L.size()-2; i<_L.size(); ++i)
   {
      _nvp += _L[i]->getNVariationalParameters();
   }
   // set the substrate in the units
   for (std::vector<NNLayer *>::size_type i=0; i<_L.size(); ++i)
   {
      _L[i]->addVariationalFirstDerivativeSubstrate(_nvp);
   }
   // set the id of the variational parameters for all the feeders
   int id_vp=0;
   for (std::vector<NNLayer *>::size_type i=_L.size()-2; i<_L.size(); ++i)
   {
      id_vp = _L[i]->setVariationalParametersID(id_vp);
   }
}


void FeedForwardNeuralNetwork::addVariationalFirstDerivativeSubstrate()
{
   // count the total number of variational parameters
   _nvp=0;
   for (std::vector<NNLayer *>::size_type i=1; i<_L.size(); ++i)
   {
      _nvp += _L[i]->getNVariationalParameters();
   }
   // set the substrate in the units
   for (std::vector<NNLayer *>::size_type i=0; i<_L.size(); ++i)
   {
      _L[i]->addVariationalFirstDerivativeSubstrate(_nvp);
   }
   // set the id of the variational parameters for all the feeders
   int id_vp=0;
   for (std::vector<NNLayer *>::size_type i=1; i<_L.size(); ++i)
   {
      id_vp = _L[i]->setVariationalParametersID(id_vp);
   }

   _flag_v1d = true;
}


void FeedForwardNeuralNetwork::addSecondDerivativeSubstrate()
{
   // add the second derivative substrate to all the layers
   for (std::vector<NNLayer *>::size_type i=0; i<_L.size(); ++i)
   {
      _L[i]->addSecondDerivativeSubstrate(_L[0]->getNUnits()-1);
   }

   _flag_2d = true;
}


void FeedForwardNeuralNetwork::addFirstDerivativeSubstrate()
{
   // set and initialize the input layer
   _L[0]->addFirstDerivativeSubstrate(_L[0]->getNUnits()-1);
   for (int i=1; i<_L[0]->getNUnits(); ++i)
   {
      _L[0]->getUnit(i)->setFirstDerivativeValue(   i-1,
           _L[0]->getUnit(i)->getActivationFunction()->f1d( _L[0]->getUnit(i)->getProtoValue() )   );
   }
   // set all the other layers
   for (std::vector<NNLayer *>::size_type i=1; i<_L.size(); ++i)
   {
      _L[i]->addFirstDerivativeSubstrate(_L[0]->getNUnits()-1);
   }

   _flag_1d = true;
}


// --- Connect the neural network

void FeedForwardNeuralNetwork::connectFFNN()
{
   for (std::vector<NNLayer *>::size_type i=1; i<_L.size(); ++i)
   {
      _L[i]->connectOnTopOfLayer(_L[i-1]);
   }
   _flag_connected = true;
}


void FeedForwardNeuralNetwork::disconnectFFNN()
{
   if ( !_flag_connected )
   {
      using namespace std;
      cout << "ERROR: FeedForwardNeuralNetwork::disconnectFFNN() : trying to disconnect an already disconnected FFNN" << endl << endl;
   }

   for (std::vector<NNLayer *>::size_type i=1; i<_L.size(); ++i)
   {
      _L[i]->disconnect();
   }
   _flag_connected = false;
}


// --- Modify NN structure

void FeedForwardNeuralNetwork::setGlobalActivationFunctions(ActivationFunctionInterface * actf)
{
   for (std::vector<NNLayer *>::size_type i=1; i<_L.size(); ++i)
   {
      _L[i]->setActivationFunction(actf);
   }
}


void FeedForwardNeuralNetwork::setLayerSize(const int &li, const int &size)
{
   _L[li]->setSize(size);
}


void FeedForwardNeuralNetwork::setLayerActivationFunction(const int &li, ActivationFunctionInterface * actf)
{
   _L[li]->setActivationFunction(actf);
}


void FeedForwardNeuralNetwork::pushHiddenLayer(const int &size)
{
   NNLayer * newhidlay = new NNLayer(size, &_log_actf);

   std::vector<NNLayer *>::iterator it = _L.end()-1;

   if (_flag_connected)
   {
      using namespace std;
      // count the number of beta before the last (output) layer
      int nbeta = 0;
      for (vector<NNLayer *>::size_type i=0; i<_L.size()-1; ++i)
      {
         for (int j=0; j<_L[i]->getNUnits(); ++j)
         {
            if (_L[i]->getUnit(j)->getFeeder())
            {
               nbeta += _L[i]->getUnit(j)->getFeeder()->getNBeta();
            }
         }
      }
      int total_nbeta = this->getNBeta();
      // store the beta for the output
      double * old_beta = new double[total_nbeta-nbeta];
      for (int i=nbeta; i<total_nbeta; ++i)
      {
         old_beta[i-nbeta] = getBeta(i);
      }

      // disconnect last layer
      _L[_L.size()-1]->disconnect();  // disconnect the last (output) layer
      // insert new layer
      _L.insert(it, newhidlay);
      // reconnect the layers
      _L[_L.size()-2]->connectOnTopOfLayer(_L[_L.size()-3]);
      _L[_L.size()-1]->connectOnTopOfLayer(_L[_L.size()-2]);

      // restore the old beta
      for (int i=nbeta; i<total_nbeta; ++i)
      {
         this->setBeta(i,old_beta[i-nbeta]);
      }
      // set all the other beta to zero
      for (int i=total_nbeta; i<this->getNBeta(); ++i)
      {
         this->setBeta(i,0.);
      }
      // set the identity activation function for some units of the new hidden layer
      for (int i=1; i<_L[_L.size()-1]->getNUnits(); ++i)
      {
         _L[_L.size()-2]->getUnit(i)->setActivationFunction(&_id_actf);
      }
      // set some beta to 1 for the output layer
      for (int i=1; i<_L[_L.size()-1]->getNUnits(); ++i)
      {
         _L[_L.size()-1]->getUnit(i)->getFeeder()->setBeta(i,1.);
      }
      // free memory
      delete[] old_beta;
   }
   else
   {
      _L.insert(it, newhidlay);
   }
}


void FeedForwardNeuralNetwork::popHiddenLayer()
{
   delete _L[_L.size()-2];

   std::vector<NNLayer *>::iterator it = _L.end()-2;
   _L.erase(it);
}


// --- Store FFNN on a file

void FeedForwardNeuralNetwork::storeOnFile(const char * filename)
{
   using namespace std;

   // open file
   ofstream file;
   file.open(filename);
   // store the number of layers
   file << getNLayers() << endl;
   // store the activaction function and size of each layer
   for (int i=0; i<getNLayers(); ++i)
   {
       file << getLayer(i)->getNUnits() << " ";
       for (int j=0; j<getLayer(i)->getNUnits(); ++j){
           file << getLayer(i)->getUnit(j)->getActivationFunction()->getIdCode() << " ";
       }
       file << endl;
   }
   // store all the variational parameters, if the FFNN is already connected
   file << _flag_connected << endl;
   if (_flag_connected){
       for (int i=0; i<this->getNBeta(); ++i)
       {
          file << getBeta(i) << " ";
       }
       file << endl;
   }
   // store the information about the substrates
   file << _flag_1d << " " << _flag_2d << " " << _flag_v1d << endl;
   file.close();
}


// --- Constructor

FeedForwardNeuralNetwork::FeedForwardNeuralNetwork(std::vector<std::vector<std::string>> &actf){
   using namespace std;

   // check input
   if (actf.size() < 3)
      throw std::invalid_argument( "There must be at least 3 layers" );
   for (vector<string> layer_actf : actf){
      if (layer_actf.size() < 2)
         throw std::invalid_argument( "Each layer must contain at least 2 units (one is for the offset)" );
   }

   // declare the NN with the right geometry
   this->construct(actf[0].size(), actf[1].size(), actf.back().size());
   for (unsigned int l=2; l<actf.size()-1; ++l){
      this->pushHiddenLayer(actf[l].size());
   }

   // set the activation functions
   ActivationFunctionInterface * af;
   for (unsigned int l=0; l<actf.size(); ++l){
      for (unsigned int u=0; u<actf[l].size(); ++u){
         af = ActivationFunctionManager::provideActivationFunction(actf[l][u]);

         if (af != 0){
            _L[l]->getUnit(u)->setActivationFunction(af);
         } else{
            cout << "ERROR FeedForwardNeuralNetwork(const int &nlayers, const int * layersize, const char ** actf) : given activation function " << actf[l][u] << " not known" << endl;
            throw std::invalid_argument( "invalid activation function id code" );
         }

         //if (actf[l][u].compare(_id_actf.getIdCode()) == 0){
         //   _L[l]->getUnit(u)->setActivationFunction(&_id_actf);
         //}
         //else if (actf[l][u].compare(_log_actf.getIdCode()) == 0){
         //   _L[l]->getUnit(u)->setActivationFunction(&_log_actf);
         //}
         //else if (actf[l][u].compare(_gss_actf.getIdCode()) == 0){
         //   _L[l]->getUnit(u)->setActivationFunction(&_gss_actf);
         //}
         //else {
         //   cout << "ERROR FeedForwardNeuralNetwork(const int &nlayers, const int * layersize, const char ** actf) : given activation function " << actf[l][u] << " not known" << endl;
         //   throw std::invalid_argument( "invalid activation function id code" );
         //}
      }
   }

}


FeedForwardNeuralNetwork::FeedForwardNeuralNetwork(const char *filename)
{
    // open file
    using namespace std;

    ifstream file;
    file.open(filename);
    string line;
    // read the number of layers
    int nlayers;
    file >> nlayers;
    // read and set the activation function and size of each layer
    string actf;
    int nunits;
    NNLayer * nnl;
    for (int i=0; i<nlayers; ++i)
    {
        file >> nunits;
        nnl = new NNLayer(nunits, &_id_actf);   // first set the activation function to the id, then change it for each unit
        for (int j=0; j<nunits; ++j){
            nnl->getUnit(j)->setActivationFunction(ActivationFunctionManager::provideActivationFunction(actf));
        }
        _L.push_back(nnl);
    }
    // connect the NN, if it is the case
    _nvp = 0;
    bool connected;
    file >> connected;
    if (connected){
        connectFFNN();
        double beta;
        for (int i=0; i<this->getNBeta(); ++i)
        {
            file >> beta;
            this->setBeta(i,beta);
        }
    }
    // read and set the substrates
    bool flag_1d, flag_2d, flag_v1d;
    file >> flag_1d;
    if (flag_1d) addFirstDerivativeSubstrate();
    file >> flag_2d;
    if (flag_2d) addSecondDerivativeSubstrate();
    file >> flag_v1d;
    if (flag_v1d) addVariationalFirstDerivativeSubstrate();

    file.close();
}


FeedForwardNeuralNetwork::FeedForwardNeuralNetwork(const int &insize, const int &hidlaysize, const int &outsize)
{
   this->construct(insize, hidlaysize, outsize);
}


void FeedForwardNeuralNetwork::construct(const int &insize, const int &hidlaysize, const int &outsize){
   NNLayer * in = new NNLayer(insize, &_id_actf);
   NNLayer * hidlay = new NNLayer(hidlaysize, &_log_actf);
   NNLayer * out = new NNLayer(outsize, &_log_actf);

   _L.push_back(in);
   _L.push_back(hidlay);
   _L.push_back(out);

   _flag_connected = false;
   _flag_1d = false;
   _flag_2d = false;
   _flag_v1d = false;

   _nvp=0;
}


// --- Destructor

FeedForwardNeuralNetwork::~FeedForwardNeuralNetwork()
{
   for (std::vector<NNLayer *>::size_type i=0; i<_L.size(); ++i)
   {
      delete _L[i];
   }
   _L.clear();

   _flag_connected = false;
   _flag_1d = false;
   _flag_2d = false;
   _flag_v1d = false;

   _nvp=0;
}
