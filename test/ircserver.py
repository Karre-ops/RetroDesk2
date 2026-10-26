# Kleiner IRC-Server fuer test/rdirc.sh: python3 test/ircserver.py PORT
# Erwartet PASS geheim, lehnt den ersten Nick ab (433), schickt PING, Thema/Namen mit UTF-8-Umlauten,
# Nachrichten, ACTION, private Nachricht, CTCP VERSION und auf "Hallo aus RD2" 30 Zeilen samt einer langen.
import socket, sys
port = int(sys.argv[1])
def log(*a): print('host:', *a, flush=True)
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', port)); s.listen(1); s.settimeout(250)
c, _ = s.accept(); log('verbunden'); f = c.makefile('rb')
def tx(z): c.sendall((z + '\r\n').encode('utf-8'))
nick = None; abgelehnt = False; user = False; da = False
while True:
    z = f.readline()
    if not z: break
    z = z.decode('utf-8', 'replace').rstrip('\r\n'); log('<', z)
    k, _, a = z.partition(' ')
    if k == 'NICK':
        if not abgelehnt: abgelehnt = True; tx(':irc.test 433 * %s :Nickname is already in use' % a); continue
        if da: tx(':%s!u@retro NICK :%s' % (nick, a)); nick = a; continue
        nick = a
    if k == 'USER': user = True
    if k in ('NICK', 'USER') and nick and user and not da:
        da = True
        tx(':irc.test 001 %s :Willkommen im RD2-Testnetz, %s' % (nick, nick))
        tx(':irc.test 004 %s irc.test test o o' % nick)
        tx(':irc.test 372 %s :- Nachricht des Tages: Grüße aus Köln' % nick)
        tx('PING :abc123')
    if k == 'JOIN':
        tx(':%s!u@retro JOIN :%s' % (nick, a))
        tx(':irc.test 332 %s %s :Thema: Retro-PCs, Grüße an alle' % (nick, a))
        tx(':irc.test 353 %s = %s :%s bob alice' % (nick, a, nick))
        tx(':irc.test 366 %s %s :End of /NAMES list.' % (nick, a))
        tx(':bob!b@nas PRIVMSG %s :Hallo RD2, schön dass du da bist' % a)
        tx(':alice!a@nas PRIVMSG %s :\x01ACTION winkt\x01' % a)
        tx(':bob!b@nas PRIVMSG %s :geheime Nachricht' % nick)
        tx(':bob!b@nas PRIVMSG %s :\x01VERSION\x01' % nick)
    if k == 'PRIVMSG' and a.endswith(':Hallo aus RD2'):
        for i in range(1, 31): tx(':bob!b@nas PRIVMSG #retro :Zeile %d' % i)
        tx(':alice!a@nas PRIVMSG #retro :' + 'Dies ist eine sehr lange Zeile, die umgebrochen werden muss, ' * 3 + 'Ende.')
    if k == 'QUIT': break
c.close(); log('zu')
