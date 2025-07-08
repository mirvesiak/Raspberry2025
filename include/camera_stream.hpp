static int wsConnect(mg_connection*, void*);
static void wsMessage(mg_connection *conn, int, char *data, size_t len, void*);

static int streamHandler(struct mg_connection *conn, void * /*cbdata*/);

void start_mjpeg_server();
void stop_mjpeg_server();