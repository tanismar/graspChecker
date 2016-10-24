#graspChecker.thrift

/**
* graspChecker_IDLServer
*
* Interface. 
*/


service graspChecker_IDLServer
{
    /**
     * Start the module
     * @return true/false on success/failure
     */
    bool start();

    /**
     * Quit the module
     * @return true/false on success/failure
     */
    bool quit();

    /**
     * Command to train the hand on empty/full examples.
     * @return true/false on success/failure to train classifiers.
     */
    bool train(1: string label = "empty");

    /**
     * Checks whether the hand is full or empty
     * @return true/false  corresponding to full or empty hand
     */
    bool check();
}
