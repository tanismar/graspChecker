/* 
 * Copyright (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Ugo Pattacini, Tanis Mar
 * email:  ugo.pattacini@iit.it, tanis.mar@iit.it
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

#include <vector>
#include <algorithm>
#include <string>
#include <fstream>
#include <iomanip>

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/math/Math.h>

#include <../include/graspChecker_IDLServer.h>

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;


/*******************************************************************************/
class GraspCheckModule : public RFModule, public graspChecker_IDLServer
{
protected:    
    BufferedPort<ImageOf<PixelRgb> > portImgIn;
    Port                            portImgOut;
    BufferedPort<Bottle> portBBcoordsIn;

    //Port portContour;    
    RpcClient portHimRep;
    RpcServer portRpc;

    std::string                         name;               //name of the module


    /*******************************************************************************/
    bool trainObserve(const string &label)
    {
        ImageOf<PixelRgb> img= *portImgIn.read(true);
        portImgOut.write(img);

        Bottle bot = *portBBcoordsIn.read(true);
        yarp::os::Bottle *items=bot.get(0).asList();

        double tlx = items->get(0).asDouble();
        double tly = items->get(1).asDouble();
        double brx = items->get(2).asDouble();
        double bry = items->get(3).asDouble();
        yInfo("[trainObserve] got bounding Box is %lf %lf %lf %lf", tlx, tly, brx, bry);

        Bottle cmd,reply;
        cmd.addVocab(Vocab::encode("train"));
        Bottle &options=cmd.addList().addList();
        options.addString(label.c_str());

        options.add(bot.get(0));

        yInfo("[trainObserve] Sending training request: %s\n",cmd.toString().c_str());
        portHimRep.write(cmd,reply);
        yInfo("[trainObserve] Received reply: %s\n",reply.toString().c_str());

        return true;
    }



    /**********************************************************/
    bool classifyObserve(string &label)
    {
        ImageOf<PixelRgb> img= *portImgIn.read(true);
        portImgOut.write(img);

        Bottle cmd,reply;
        cmd.addVocab(Vocab::encode("classify"));
        Bottle &options=cmd.addList();

        Bottle bot = *portBBcoordsIn.read(true);

        for (int i=0; i<bot.size(); i++)
        {
            ostringstream tag;
            tag<<"blob_"<<i;
            Bottle &item=options.addList();
            item.addString(tag.str().c_str());
            item.addList()=*bot.get(i).asList();
        }

        yInfo("[classifyObserve] Sending classification request: %s\n",cmd.toString().c_str());
        portHimRep.write(cmd,reply);
        yInfo("[classifyObserve] Received reply: %s\n",reply.toString().c_str());

        label = processScores(reply);

        yInfo("[classifyObserve] the recognized label is %s", label.c_str());

        return true;
    }
    /**********************************************************/
    string processScores(const Bottle &scores)
    {

        double maxScoreObj=0.0;
        string label  ="";

        for (int i=0; i<scores.size(); i++)
        {
            ostringstream tag;
            tag<<"blob_"<<i;

            Bottle *blobScores=scores.find(tag.str().c_str()).asList();
            if (blobScores==NULL)
                continue;

            for (int j=0; j<blobScores->size(); j++)
            {
                Bottle *item=blobScores->get(j).asList();
                if (item==NULL)
                    continue;

                string name=item->get(0).asString().c_str();
                double score=item->get(1).asDouble();

                yInfo("name is %s with score %f", name.c_str(), score);

                if (score>maxScoreObj)
                {
                    maxScoreObj = score;
                    label.clear();
                    label = name;
                }

            }
        }
        return label;
    }

public:
    /*******************************************************************************/
    bool configure(ResourceFinder &rf)
    {
        name = rf.check("name", Value("graspChecker"), "Getting module name").asString();


        printf("Opening ports\n" );
        bool ret= true;

        ret = ret && portBBcoordsIn.open("/"+name+"/bb:i");
        ret = ret && portImgIn.open("/"+name+"/img:i");
        ret = ret && portImgIn.open("/"+name+"/img:o");

        ret = ret && portHimRep.open("/"+name+"/himrep:rpc");
        ret = ret && portRpc.open("/"+name+"/rpc:i");

        if (!ret){
            printf("Problems opening ports\n");
            return false;
        }
        printf("Ports opened\n");

        attach(portRpc);

        return true;
    }

    /*******************************************************************************/
    bool interruptModule()
    {
        portBBcoordsIn.interrupt();
        portImgIn.interrupt();
        portImgOut.interrupt();

        portHimRep.interrupt();
        portRpc.interrupt();
        return true;
    }

    /*******************************************************************************/
    bool close()
    {
        portBBcoordsIn.close();
        portImgIn.close();
        portImgOut.close();

        portHimRep.close();
        portRpc.close();
        return true;
    }

    /*******************************************************************************/
    double getPeriod()
    {
        return 0.1;
    }

    /*******************************************************************************/
    bool updateModule()
    {        
        return true;
    }


    /* =========================================== RPC COMMANDS ========================================= */

    //Thrifted
    bool train(const string &label)
    {
        return trainObserve(label);
    }

    bool classify()
    {
        string label;
        classifyObserve(label);

        bool answer;
        if (strcmp (label.c_str(),"full") == 0)
            answer = true;
        else
            answer = false;


        //answer = false;

        return answer;
    }
};


/*******************************************************************************/
int main(int argc,char *argv[])
{   
    Network yarp;
    if (!yarp.checkNetwork())
    {
        yError("unable to find YARP server!");
        return 1;
    }

    GraspCheckModule mod;
    ResourceFinder rf;
    rf.setDefaultContext("checkGrasp");
    rf.configure(argc,argv);
    return mod.runModule(rf);
}

