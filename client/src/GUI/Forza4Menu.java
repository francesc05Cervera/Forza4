package GUI;
import javax.swing.*;
import java.awt.*;
import java.io.*;
import java.net.*;

public class Forza4Menu {
    
    private JFrame frame;
    private Socket socket;
    private PrintWriter out;
    private BufferedReader in;
    private JTextArea areaNotifiche; // La nuova bacheca visiva
    private volatile boolean inGioco = false; // Flag per stoppare l'ascolto quando andiamo sulla griglia

    public Forza4Menu() {
        // 1. CONNESSIONE AL SERVER
        try {
            socket = new Socket("127.0.0.1", 9090);
            out = new PrintWriter(socket.getOutputStream(), true);
            in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
        } catch (Exception e) {
            JOptionPane.showMessageDialog(null, 
                "❌ Impossibile connettersi al Server!\nAssicurati che il container Docker sia avviato.", 
                "Errore di Rete", JOptionPane.ERROR_MESSAGE);
            System.exit(0); 
        }

        // 2. CREAZIONE INTERFACCIA GRAFICA
        frame = new JFrame("Forza 4 - Menu Principale");
        frame.setSize(450, 400);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setLayout(new BorderLayout(10, 10)); 

        // Pannello Superiore (Titolo e Pulsanti)
        JPanel pannelloSuperiore = new JPanel(new GridLayout(3, 1, 5, 5));
        JLabel titolo = new JLabel("Benvenuto in Forza 4", SwingConstants.CENTER);
        titolo.setFont(new Font("Arial", Font.BOLD, 18));
        JButton btnCrea = new JButton("Crea Nuova Partita");
        JButton btnUnisci = new JButton("Unisciti a una Partita");
        
        pannelloSuperiore.add(titolo);
        pannelloSuperiore.add(btnCrea);
        pannelloSuperiore.add(btnUnisci);
        frame.add(pannelloSuperiore, BorderLayout.NORTH);

        // Pannello Centrale (La Bacheca delle Notifiche di Sistema)
        JPanel pannelloBacheca = new JPanel(new BorderLayout());
        pannelloBacheca.setBorder(BorderFactory.createTitledBorder("📢 Bacheca Server (Stato del Sistema)"));
        areaNotifiche = new JTextArea();
        areaNotifiche.setEditable(false);
        areaNotifiche.setBackground(Color.BLACK);
        areaNotifiche.setForeground(Color.GREEN); // Stile terminale retro molto carino
        areaNotifiche.setFont(new Font("Consolas", Font.PLAIN, 12));
        JScrollPane scroll = new JScrollPane(areaNotifiche);
        pannelloBacheca.add(scroll, BorderLayout.CENTER);
        frame.add(pannelloBacheca, BorderLayout.CENTER);

        // 3. AZIONI DEI PULSANTI (Inviano solo il comando, non aspettano in modo bloccante!)
        btnCrea.addActionListener(e -> out.println("CREATE"));

        btnUnisci.addActionListener(e -> {
            String idInput = JOptionPane.showInputDialog(frame, "Inserisci l'ID della partita a cui vuoi unirti:");
            if (idInput != null && !idInput.trim().isEmpty()) {
                out.println("JOIN;" + idInput.trim());
            }
        });

        frame.setLocationRelativeTo(null);
        frame.setVisible(true);

        // 4. IL THREAD IN ASCOLTO SUL MENU (Gestione Asincrona del traffico)
        new Thread(() -> {
            try {
                String messaggio;
                // Finché non entriamo in partita, leggiamo tutto quello che capita nel socket
                while (!inGioco && (messaggio = in.readLine()) != null) {
                    System.out.println("Menu Ricevuto: " + messaggio);
                    final String msgCodificato = messaggio;

                    // Caso A: È una notifica di broadcast per la bacheca
                    if (msgCodificato.startsWith("NOTIFICA")) {
                        String testoNotifica = msgCodificato.split(";")[1];
                        SwingUtilities.invokeLater(() -> areaNotifiche.append("• " + testoNotifica + "\n"));
                    }
                    // Caso B: Il server ci conferma che abbiamo creato la partita con successo
                    else if (msgCodificato.startsWith("PARTITA_CREATA")) {
                        int idPartita = Integer.parseInt(msgCodificato.split(";")[1]);
                        inGioco = true; // Fermiamo questo thread loop
                        SwingUtilities.invokeLater(() -> {
                            frame.dispose(); // Chiudiamo il menu
                            new Forza4Griglia(idPartita, out, in); // Passiamo il controllo alla griglia
                        });
                    }
                    // Caso C: L'avversario ha accettato e siamo entrati
                    else if (msgCodificato.startsWith("UNISCITI_OK")) {
                        int idPartita = Integer.parseInt(msgCodificato.split(";")[1]);
                        inGioco = true;
                        SwingUtilities.invokeLater(() -> {
                            frame.dispose();
                            new Forza4Griglia(idPartita, out, in);
                        });
                    }
                    // Caso D: Errore (partita piena, rifiutata, ecc.)
                    else if (msgCodificato.startsWith("ERRORE")) {
                        String messaggioErrore = msgCodificato.split(";")[1];
                        SwingUtilities.invokeLater(() -> JOptionPane.showMessageDialog(frame, "❌ " + messaggioErrore, "Attenzione", JOptionPane.WARNING_MESSAGE));
                    }
                }
            } catch (IOException ex) {
                System.out.println("Connessione menu interrotta.");
            }
        }).start();
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new Forza4Menu());
    }
}