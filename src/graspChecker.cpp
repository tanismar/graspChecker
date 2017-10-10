
#include "graspChecker.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;

/**********************************************************
                    PRIVATE METHODS
/**********************************************************/



/*******************************************************************************/
bool GraspCheckModule::trainObserve(const string &label, BoundingBox &bb)
{
    cout<< " Training image with label " << label << endl;
    ImageOf<PixelRgb> img= *portImgIn.read(true);
    portImgOut.write(img);

    Bottle bot;
    // Check if Bounding box was not initialized
    if ((bb.tlx + bb.tly + bb.brx + bb.bry) == 0.0 ){
        bot = *portBBcoordsIn.read(true);            // read all bounding boxes
        Bottle *items=bot.get(0).asList();                  // pick up first bounding box

        bb.tlx = items->get(0).asDouble();
        bb.tly = items->get(1).asDouble();
        bb.brx = items->get(2).asDouble();
        bb.bry = items->get(3).asDouble();
    }else{

        Bottle &items = bot.addList();
        items.addDouble(bb.tlx);
        items.addDouble(bb.tly);
        items.addDouble(bb.brx);
        items.addDouble(bb.bry);
    }

    yInfo("[trainObserve] got bounding Box is %lf %lf %lf %lf", bb.tlx, bb.tly, bb.brx, bb.bry);

    Bottle cmd,reply;
    cmd.addVocab(Vocab::encode("train"));

    Bottle &options=cmd.addList().addList();        // Add a bottle in bottle: cmd = "train (( ))"
    options.addString(label.c_str());               // Add label             : cmd = "train (( label ))"
    options.add(bot.get(0));                        // Add 'items' bottle    : cmd = "train ( (label(tlx tly brx bry)) )"

    yInfo("[trainObserve] Sending training request: %s\n",cmd.toString().c_str());
    portHimRep.write(cmd,reply);
    yInfo("[trainObserve] Received reply: %s\n",reply.toString().c_str());

    return true;
}



/**********************************************************/
bool GraspCheckModule::classifyObserve(string &label, BoundingBox &bb)
{
    ImageOf<PixelRgb> img= *portImgIn.read(true);
    portImgOut.write(img);

    Bottle cmd,reply;
    cmd.addVocab(Vocab::encode("classify"));        // cmd = classify

    Bottle &options=cmd.addList();                  // cmd = classify ()

    // Check if Bounding box was not initialized
    if ((bb.tlx + bb.tly + bb.brx + bb.bry) == 0.0 ){
        Bottle bot = *portBBcoordsIn.read(true);        // Read all Bounding boxes. Bot = ((bb1)(bb2)(bb3))
        for (int i=0; i<bot.size(); i++)                // Add each of them
        {
            ostringstream tag;
            tag<<"blob_"<<i;
            Bottle &item=options.addList();             // cmd = classify (() () () )
            item.addString(tag.str().c_str());          // cmd = classify ((blob_i) (blob_j) (...) )
            item.addList()=*bot.get(i).asList();        // cmd = classify ((blob_i (bb1)) (blob_j (bb2)) (...) )
        }
    }else{
        Bottle &item=options.addList();                 // cmd = classify (())
        item.addString("blob_0");                       // cmd = classify ((blob_i))
        Bottle &bbox = item.addList();                  // cmd = classify ((blob_i ()))
        bbox.addDouble(bb.tlx);
        bbox.addDouble(bb.tly);
        bbox.addDouble(bb.brx);
        bbox.addDouble(bb.bry);                         // cmd = classify ((blob_i(bb1)))
    }

    yInfo("[classifyObserve] Sending classification request: %s\n",cmd.toString().c_str());
    portHimRep.write(cmd,reply);
    yInfo("[classifyObserve] Received reply: %s\n",reply.toString().c_str());

    label = processScores(reply);

    yInfo("[classifyObserve] the recognized label is %s", label.c_str());

    return true;
}


/**********************************************************/
string GraspCheckModule::processScores(const Bottle &scores)
{
    cout << " Processing the scores" << endl;
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

/*******************************************************************************/
bool GraspCheckModule::configure(ResourceFinder &rf)
{
    name = rf.check("name", Value("graspChecker"), "Getting module name").asString();
    running = true;

    printf("Opening ports\n" );
    bool ret= true;

    ret = ret && portBBcoordsIn.open("/"+name+"/bb:i");
    ret = ret && portImgIn.open("/"+name+"/img:i");
    ret = ret && portImgOut.open("/"+name+"/img:o");

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
bool GraspCheckModule::interruptModule()
{
    portBBcoordsIn.interrupt();
    portImgIn.interrupt();
    portImgOut.interrupt();

    portHimRep.interrupt();
    portRpc.interrupt();
    return true;
}

/*******************************************************************************/
bool GraspCheckModule::close()
{
    portBBcoordsIn.close();
    portImgIn.close();
    portImgOut.close();

    portHimRep.close();
    portRpc.close();

    running = false;
    return true;
}

/*******************************************************************************/
double GraspCheckModule::getPeriod()
{
    return 0.1;
}

/*******************************************************************************/
bool GraspCheckModule::updateModule()
{
    if (!running)
        return false;
    return true;
}


/* =========================================== RPC COMMANDS ========================================= */

/******************RPC HANDLING METHODS*******************/
bool GraspCheckModule::attach(yarp::os::RpcServer &source)
{
    return this->yarp().attachAsServer(source);
}

/* Atomic commands */
// Setting up commands
bool GraspCheckModule::start(){
    std::cout << "Starting!" << std::endl;
    running = true;
    return true;
}
bool GraspCheckModule::quit(){
    std::cout << "Quitting!" << std::endl;
    running = false;
    return true;
}

//Thrifted
bool GraspCheckModule::train(const string &label, const double tlx ,const double tly, const double brx, const double bry)
{
    BoundingBox bb;
    bb.tlx = tlx;
    bb.tly = tly;
    bb.brx = brx;
    bb.bry = bry;

    return trainObserve(label, bb);
}

bool GraspCheckModule::check(const double tlx ,const double tly, const double brx, const double bry)
{
    string label;

    BoundingBox bb;
    bb.tlx = tlx;
    bb.tly = tly;
    bb.brx = brx;
    bb.bry = bry;

    cout << "Classifying image from crop " << tlx <<", "<< tly <<", "<< brx <<", "<< bry <<". "<<endl;

    classifyObserve(label, bb);

    cout<< " classifyObserve returned "<< label << endl;

    bool answer;
    if (strcmp (label.c_str(),"full") == 0)
        answer = true;
    else
        answer = false;

    //answer = false;

    return answer;
}


/*******************************************************************************/
int main(int argc,char *argv[])
{   
    Network yarp;
    if (!yarp.checkNetwork())
    {
        yError("unable to find YARP server!");
        return 1;
    }

    GraspCheckModule graspCheck;
    ResourceFinder rf;
    rf.setDefaultContext("graspChecker");
    rf.configure(argc,argv);
    return graspCheck.runModule(rf);
}

