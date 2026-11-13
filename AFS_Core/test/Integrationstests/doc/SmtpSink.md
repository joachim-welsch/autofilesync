# SmtpSink

Module comment for SmtpSink.h / SmtpSink.cpp.

## SmtpSink

A minimal SMTP server in the test process, so the real send_mail() can be driven end to end
without a mail server, a network or TLS: it greets, answers EHLO, refuses STARTTLS with 502,
refuses AUTH, accepts MAIL FROM, answers RCPT TO with a configurable reply, collects DATA up to
the final "." and says goodbye on QUIT. SmtpSession continues in plain text after a refused
STARTTLS only when the account has no password - that is exactly the path the sink serves, and
PasswordIsNeverSentWithoutTls pins the other side of it. Everything the client sent is recorded,
so a test can also assert what did *not* happen (no AUTH, no DATA). Unix sockets only; the
Windows build gets the declarations without a body.

## ephemeral-port

Port 0 lets the kernel pick a free port, read back with getsockname(); a fixed port would
collide with a second test run or a real mail service.

## account

The account that reaches the sink: loopback address, the sink's port, no user, no password,
a fixed sender. Tests change what they need on the copy.

## serve

One thread accepts connections in a 50 ms poll loop and serves them one after another; the
poll is what lets the destructor stop the thread without a trick like connecting to itself.
A connection counts as handled only after its session ended, so wait_for_connections() also
tells a test that the mail has been recorded.

## StartTls

The second kind of sink: it answers STARTTLS with 220 and continues in TLS, and after that it
serves AUTH LOGIN, recording every user and password and accepting the one pair given with
accept_login(). Its certificate is made at construction - a key pair and a self-signed
certificate for 127.0.0.1, valid for an hour - and handed out as PEM so that a test can make
it trusted. The plain sink is unchanged and still refuses both commands.

## tls_

The TLS layer of the connection being served, or null before STARTTLS. One connection is
served at a time, so one member is enough. Reading asks SSL_pending() before it polls the
socket: a TLS record that was already read from the socket holds lines the socket no longer
announces.
