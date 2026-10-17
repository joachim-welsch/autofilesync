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
