# astm
ASTM communication for medical equipment in C\\
Written in C\\
need libevent library\\
need /root/inbox and /root/outbox folder\\
One file communicate with equipment (erba-XL-640)\\
  it store received data in outbox\\
  it reads inbox every 10 seconds and if any file there, send it to instrument\\
  \\
  \\
  manage_box read inbox\\
    if data is query, reads sample information from mysql and write responce in outbox\\
    if data is result, stores it in mysql\\
    \\
  can handle one sample query at a time\\
