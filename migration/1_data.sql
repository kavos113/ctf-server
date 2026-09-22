USE ctf_server;

-- Development-only account: username=user1, password=password123
INSERT INTO users (id, username, password_hash, created_at)
VALUES ('00000000000000000000000000000001', 'user1', '$argon2id$v=19$m=65536,t=2,p=1$vjuAQ4alumuvPPcPbljrgQ$RDvekCBbObjoQz0LOZC53NBoliraG5JqaXd3qO4zlYA', UTC_TIMESTAMP());

INSERT INTO challenges (creator_id, name, description, flag, genre) VALUES ('00000000000000000000000000000001', 'sample-problem', 'sample-problem-description', 'flag{HAPPY}', 1);
