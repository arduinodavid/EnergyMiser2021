void send_eMail(String textMsg, String toName, String toEmail) {

    if (toEmail.length() == 0) return;

    sprintf(strMsg, "Sending email to %s", toEmail.c_str()); sendMessage(strMsg);

    smtp.debug(1); // 1 for verbose
    smtp.callback(smtpCallback);

    ESP_Mail_Session session;
    session.server.host_name = SMTP_HOST;
    session.server.port = SMTP_PORT;
    session.login.email = AUTHOR_EMAIL;
    session.login.password = AUTHOR_PASSWORD;

    SMTP_Message message;
    message.sender.name = "Energy Miser";
    message.sender.email = AUTHOR_EMAIL;
    message.subject = "Energy Miser Status";
    message.addRecipient(toName.c_str(), toEmail.c_str());
    message.text.content = textMsg.c_str();

    message.text.charSet = "us-ascii";
    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
    message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

    if (!smtp.connect(&session)) return;

    if (!MailClient.sendMail(&smtp, &message))
        Serial.println("Error sending Email, " + smtp.errorReason());
}

// Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status) {
    return;

    /* Print the current status */
    Serial.println(status.info());

    /* Print the sending result */
    if (status.success())
    {
        Serial.println("----------------");
        Serial.printf("Message sent success: %d\n", status.completedCount());
        Serial.printf("Message sent failled: %d\n", status.failedCount());
        Serial.println("----------------\n");
        struct tm dt;

        for (size_t i = 0; i < smtp.sendingResult.size(); i++)
        {
            /* Get the result item */
            SMTP_Result result = smtp.sendingResult.getItem(i);
            localtime_r(&result.timesstamp, &dt);

            Serial.printf("Message No: %d\n", i + 1);
            Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
            Serial.printf("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
            Serial.printf("Recipient: %s\n", result.recipients);
            Serial.printf("Subject: %s\n", result.subject);
        }
        Serial.println("----------------\n");
    }
}

