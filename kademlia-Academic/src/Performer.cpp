#include "Performer.hpp"

static inline void startSearch(const Key* key, Performer* p, const Message* msg)
{

    //find the closest nodes known to send the message
    Kbucket bucket;
    p->neighbours->findKClosestNodes(key, &bucket);
    if (bucket.getSize() == 0)
    {
        std::cout << "WARNING: no node found in all kbuckets" << std::endl;
        return;
    }

    //construct the SearchNode in place inside the hashmap
    std::pair<std::unordered_map<Key,SearchNode>::iterator,bool> res =
    p->searchInProgress.emplace(std::piecewise_construct, std::make_tuple(*key),
                                std::make_tuple(key,&bucket));
    //retrieve the first three nodes to query
    Node askto[ALPHA_REQUESTS];
    int retval = res.first->second.queryTo(askto);

    for (int i = 0; i < retval; i++)
    {
        (Messenger::getInstance()).sendMessage(askto[i], *msg);
    }
}

void rpc_ping(Node node)
{
    //create message
    Message response(RPC_PING);

    //send PING
    (Messenger::getInstance()).sendMessage(node, response);
}

void rpc_pong(Node node)
{
    //create message
    Message response(RPC_PING | FLAG_ANSWER);

    //send PONG
    (Messenger::getInstance()).sendMessage(node, response);
}

void rpc_store(const Key* key, const Kbucket* bucket, Performer* p)
{
    Message response(key->getKey(), NBYTE);
    std::string value;
    std::unordered_map<Key, const std::string>::const_iterator got =
    p->storeTmpMap.find(*key);
    if(got != p->storeTmpMap.end())
    {
        value = std::string(got->second);
        p->storeTmpMap.erase(got);
    }
    else
        return;

    response.append((uint8_t*) value.c_str(), strlen(value.c_str()) + 1);
    response.setFlags(RPC_STORE);
    Logger::getInstance().logBoth("ssst","Storing the value: ",value.c_str(),
                                  "in the following bucket: ",bucket);
    std::list<Node>::const_iterator it;
    for(it = bucket->getNodes()->begin(); it != bucket->getNodes()->end(); ++it)
    {
        Messenger::getInstance().sendMessage(*it, response);
    }
}

void rpc_store_request(const std::string& value, Performer* p)
{
    //store text temporarily
    Key key(value.c_str());
    p->storeTmpMap.insert(std::make_pair(key, value));

    //create store message
    Message storeMsg = generate_find_node_request(&key);
    storeMsg.setFlags(storeMsg.getFlags() | FLAG_STORE_REQUEST);
    startSearch(&key, p, &storeMsg);
}

void rpc_find_node(const Key* key, Performer* p)
{    
    Message findNodeMsg = generate_find_node_request(key);
    findNodeMsg.setFlags(findNodeMsg.getFlags());

    startSearch(key, p, &findNodeMsg);
}

void rpc_find_value(const Key* key, Performer* p)
{

    if(p->myselfHasValue(key))
        return;

    Message findValueMsg = generate_find_node_request(key);
    findValueMsg.setFlags(findValueMsg.getFlags() | FLAG_FIND_VALUE);

    startSearch(key, p, &findValueMsg);
}

Message generate_find_node_request(const Key* key)
{
    Message response(key->getKey(), NBYTE);
    response.setFlags(RPC_FIND_NODE);
    return response;
}

Message generate_find_node_request(const Node findme)
{
    return generate_find_node_request(findme.getKey());
}

Message generate_find_node_answer(const Key* key, Kbucket* bucket)
{
    uint8_t data[500-NBYTE];
    Message response(key->getKey(), NBYTE);
    int len = bucket->serialize(data);

    response.append(data,len);
    response.setFlags(RPC_FIND_NODE | FLAG_ANSWER);
    return response;
}

static void* execute(void* this_class)
{
    Performer* p = (Performer*)this_class;
    Messenger* messenger = &(Messenger::getInstance());
    Logger* logger = &(Logger::getInstance());
    std::queue<Message*>* q = p->message_queue;
    Node me(messenger->getIp(),messenger->getPort());
    while (true)
    {
        Messenger* m = &(Messenger::getInstance());
        std::unique_lock<std::mutex> mlock(m->mutex);

        while (p->message_queue->size() == 0)
        {
            m->cond_var.wait(mlock);
        }
        Message* top;
        top = q->front();
        q->pop();
        if(top==NULL)
            continue;
        Node sender = top->getSenderNode();

        //update bucket -> add the sender node whichever the RPC is
        p->neighbours->insertNode(sender);
        uint8_t flags = top->getFlags();
        bool isAnswer = flags & FLAG_ANSWER;
        if(sender!=me)
        {
            logger->logStdout("ssnsf", LOGGER_INCOMING,
            "Message from", &sender, "with flags:", &flags);
            // logger->logStdout("sn","Received a message from", &sender);
        }
        else
        {
            //waiting for pending nodes
            //SearchNode clean??????????????????????????????????????????
            logger->logStdout("ssnsf", LOGGER_INCOMING,
            "Message from me", &sender, "with flags:", &flags);
        }
        switch(flags & RPC_MASK)
        {
            case RPC_PING :
            {
                if(!isAnswer)
                {
                    logger->logStdout("s","Received a ping");
                    rpc_pong(sender);
                }
                else
                {
                    logger->logStdout("s","Received a pong");
                    Updater::getInstance().processPong(sender);
                }
            }
                break;
            case RPC_STORE :
            {
                logger->logStdout("s","Received a store");

                Key key;
                key.craft(top->getData());
                short textLength = top->getLength();
                //???text?????????key???value
                char* text = new char[textLength - NBYTE];
                for(int i = NBYTE; i < textLength; i++)
                {
                    text[i - NBYTE] = top->getText()[i];
                }
                if(p->filesMap.insert(std::make_pair(key,text)).second == false)
                {
                    logger->logBoth("s","ERROR: key already inserted");
                }
            }
                break;
            case RPC_FIND_NODE :
            {
                if(!isAnswer)
                {
                    Key key;
                    key.craft(top->getData());
                    //find closest nodes
                    logger->logStdout("sk","Somebody asked Kbucket of :",&key);
                    if(flags & FLAG_FIND_VALUE)
                    {
                        std::unordered_map<Key,const char*>::iterator got =
                        p->filesMap.find(key);
                        if(got!=p->filesMap.end())
                        {
                            Message msg(key.getKey(),NBYTE);
                            msg.append((const uint8_t*)got->second,
                                       strlen(got->second)+1);
                            msg.setFlags(RPC_FIND_NODE|FLAG_ANSWER|
                                         FLAG_FIND_VALUE|FLAG_VALUE_FOUND);
                            messenger->sendMessage(sender,msg);
                            break;
                        }
                    }
                    //find closest nodes
                    Kbucket kbucket;
                    p->neighbours->findKClosestNodes(&key, &kbucket);
                   kbucket.print();
                    Message msg = generate_find_node_answer(&key, &kbucket);
                    msg.setFlags(msg.getFlags()|(flags&~RPC_MASK));   //??????????????????RPC_MASK?????????flag
                    messenger->sendMessage(sender, msg);
                }
                else //answer?????????????????????
                {
                    Key k;    
                    k.craft(top->getData());   //???????????????key
                    if(sender!=me) 
                        logger->logStdout("sk","Received answer for key: ",&k);
                    SearchNode* sn = NULL;
                    std::unordered_map<Key,SearchNode>::iterator got =
                    p->searchInProgress.find(k);

                    //SearchNode found, otherwise received a message from a
                    //queried node, but the Kbucket has been completed
                    if(got != p->searchInProgress.end())
                    {
                        sn = &(got->second);
                        //value found, the find value can terminate
                        if(flags&FLAG_VALUE_FOUND)
                        {
                            logger->logBoth("sksss", "Found value  - Key:",&k,
                                              "Value:",top->getData()+NBYTE);
                            p->searchInProgress.erase(k);
                            break;
                        }

                        Kbucket b(top->getData()+NBYTE);      //??????k-buket??????node
                        sn->addAnswer(sender, &b);
                        Node askto[ALPHA_REQUESTS];
                        int retval = sn->queryTo(askto);
                        if (retval > 0) //need to query somebody
                        {
                            Message msg = generate_find_node_request(&k);   // ?????????????????????key
                            msg.setFlags(msg.getFlags() |
                                        (top->getFlags() &
                                         ~(RPC_MASK|FLAG_ANSWER)));      //RPC_FIND_NODE ?????? ??????RPC??? FLAG_ANSWER
                            for (int i = 0; i < retval; i++)
                                messenger->sendMessage(askto[i],msg);
                        }
                        else if (retval == 0) //Kbucket ready   ??????ACTIVE?????????????????????
                        {
                            Kbucket res;
                            sn->getAnswer(&res);
                            p->searchInProgress.erase(got);
                            logger->logStdout("skst","KBucket for key ",
                                              &k," result: ",&res);
                            if(flags&FLAG_STORE_REQUEST)
                            {
                                //send the rpc_store to the node in the now
                                //completed Kbucket
                                rpc_store(&k, &res, p);
                            }
                        } else {
                            //need to wait the pending nodes
                            messenger->sendMessage(me,*top);
                        }
                    }
                    else //search node not found. So it was already processed.
                        ;
                    //   -> Bail out
                }
            }
                break;
            default:
                //ignore the packet with wrong type flag
                ;
        }
        delete top;
    }
    //pthread_exit((void*)0); <- will never be executed
}

//clean every SearchNode periodically
static void* pendingNodesCleaner(void* data)
{
    std::unordered_map<Key, SearchNode>* searchInProgress =
    (std::unordered_map<Key,SearchNode>*)data;
    std::unordered_map<Key, SearchNode>::iterator it;
    SearchNode* target;
    while(true)
    {
        it = searchInProgress->begin();
        while(it!=searchInProgress->end())
        {
            target = &(it->second);
            target->clean();
            it++;
        }
        sleep(TIMEOUT);
    }
    //pthread_exit((void*)0); <- will never be executed
}

Performer::Performer(std::queue<Message*>* q)
{
    Messenger* m = &(Messenger::getInstance());

    Node* myself = new Node(m->getIp(), m->getPort());
    Performer::neighbours = new NeighbourManager(*myself);

    Performer::message_queue = q;
    pthread_create(&(Performer::thread_id), NULL, execute, (void*)this);
    pthread_create(&(Performer::cleaner_id), NULL,
                   pendingNodesCleaner,&(Performer::searchInProgress));
}

const pthread_t Performer::getThreadID()const
{
    return Performer::thread_id;
}

Performer::~Performer()
{
    pthread_kill(Performer::cleaner_id,0);
    pthread_kill(Performer::thread_id,0);
    delete neighbours;
}

//not the best routine, just used to know if I cant print at least part of data
static bool isPrintable(const uint8_t* data)
{
    for(int i=0;i<500;i++)
    {
        if(data[i]=='\0') //null-terminated
            return true;
        if(data[i]<32) //binary
            return false;
    }
    return true;
}

void Performer::printFilesMap()
{
    std::cout << "---------- TEXT VALUES STORED ON THIS SERVER ----------"
              << std::endl;
    std::unordered_map<Key,const char*>::iterator it;
    for (it = filesMap.begin(); it != filesMap.end(); ++it)
    {
        if(isPrintable((const uint8_t*)(it->second)))
        {
            std::cout << "KEY: ";
            it->first.print();
            std::cout << "TEXT: " << it->second << std::endl << std::endl;
        }
    }
    std::cout << "-------------------------------------------------------"
              << std::endl;
}

bool Performer::myselfHasValue(const Key* key) {

    std::unordered_map<Key,const char*>::const_iterator got =
    filesMap.find(*key);
    if (got != filesMap.end())
    {
        std::cout << "Found value: " << got->second << std::endl;
        return true;
    }
    else
    {
        return false;
    }
}
