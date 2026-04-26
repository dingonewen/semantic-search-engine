**I. searchserver main (server port, directory)**

1. build m_index
2. set kill to false
3. create n threads
4. create server **socket address**, **bind** claims the server port, **listen** tells the OS to start queuing incoming connection requests from client
5. while not kill {

        accept unblocked when there's a connection requests, acquire client_fd
        pack the request and dispatch it to m_work_queue

        worker thread keep looping and when notified by parent and obtained the lock handle:
            GET / -> home page
            GET /query?terms= -> search result
            GET /static/test_tree/... -> serve a file
            PUT /static/test_tree/... -> replace/upload a file (updates m_index)
            POST /static/test_tree/... -> upload a file (updates m_index)
            DELETE /static/test_tree/... -> delete a file (updates m_index)
   }
6. Ctrl+C lets OS deliver SIGINT, set m_done to 1 and kill the server

**II. searchclient main (server ip, server port)**

Create server **socket address**, **connect** to server, **send** request, **recv** data from server socket

1. send: 

        PUT/POST body

        GET/DELETE {}

2. recv: 

        GET body

        PUT/POST/DELETE {}
          
**III. web client**

By web broswer: create server **socket address**, **connect** to server, **send** query and static request, **recv** data from server socket 
