# Import smtplib for the actual sending function
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
import smtplib

# Import the email modules we'll need
from email.mime.text import MIMEText

def send_mail(subject, from_address, to_address, message):
    msg = MIMEText(message)

    msg['Subject'] = subject
    msg['From'] = from_address
    msg['To'] = to_address

    # Send the message via our own SMTP server, but don't include the
    # envelope header.
    s = smtplib.SMTP('localhost')
    s.sendmail(from_address, [to_address], msg.as_string())
    s.quit()
