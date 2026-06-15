package GUI;
import javax.swing.*;
import java.awt.*;
import java.io.*;

public class Forza4Griglia {
    private JFrame frame;
    private JButton[][] bottoni;
    private PrintWriter out;
    private BufferedReader in; 
    private int idPartita;

    public Forza4Griglia(int idPartita, PrintWriter out, BufferedReader in) {
        this.idPartita = idPartita;
        this.out = out;
        this.in = in; 

        frame = new JFrame("Forza 4 - Partita ID: " + idPartita);
        frame.setSize(700, 600);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setLayout(new GridLayout(6, 7, 5, 5)); 
        frame.getContentPane().setBackground(Color.BLUE); 

        bottoni = new JButton[6][7];

        for (int r = 0; r < 6; r++) {
            for (int c = 0; c < 7; c++) {
                bottoni[r][c] = new JButton();
                bottoni[r][c].setBackground(Color.WHITE); 
                bottoni[r][c].setOpaque(true);
                bottoni[r][c].setBorderPainted(false);
                
                final int colonnaScelta = c;
                
                bottoni[r][c].addActionListener(e -> {
                    out.println("MOVE;" + colonnaScelta);
                });
                
                frame.add(bottoni[r][c]);
            }
        }

        frame.setLocationRelativeTo(null);
        frame.setVisible(true);

        avviaAscoltoServer();
    }

    private void avviaAscoltoServer() {
        new Thread(() -> {
            try {
                String messaggio;
                while ((messaggio = in.readLine()) != null) {
                    System.out.println("Ricevuto dal server: " + messaggio);
                    
                    if (messaggio.startsWith("UPDATE")) {
                        String[] parti = messaggio.split(";");
                        int r = Integer.parseInt(parti[1]);
                        int c = Integer.parseInt(parti[2]);
                        int giocatore = Integer.parseInt(parti[3]);

                        SwingUtilities.invokeLater(() -> {
                            if (giocatore == 1) {
                                bottoni[r][c].setBackground(Color.RED);
                            } else {
                                bottoni[r][c].setBackground(Color.YELLOW);
                            }
                        });
                    }
                    else if (messaggio.startsWith("RICHIESTA_JOIN")) {
                        SwingUtilities.invokeLater(() -> {
                            int risposta = JOptionPane.showConfirmDialog(frame, 
                                "Un giocatore ha chiesto di unirsi alla tua partita.\nVuoi accettarlo?", 
                                "Richiesta di Accesso", 
                                JOptionPane.YES_NO_OPTION);
                                
                            if (risposta == JOptionPane.YES_OPTION) {
                                out.println("RISPOSTA_JOIN;SI");
                            } else {
                                out.println("RISPOSTA_JOIN;NO");
                            }
                        });
                    }
                    else if (messaggio.equals("VITTORIA")) {
                        SwingUtilities.invokeLater(() -> gestisciFinePartita("🎉 HAI VINTO! Complimenti!\nVuoi giocare ancora?", "Vittoria"));
                    }
                    else if (messaggio.equals("SCONFITTA")) {
                        SwingUtilities.invokeLater(() -> gestisciFinePartita("💀 HAI PERSO! L'avversario ti ha battuto.\nVuoi giocare ancora?", "Sconfitta"));
                    }
                    else if (messaggio.equals("PAREGGIO")) {
                        SwingUtilities.invokeLater(() -> gestisciFinePartita("🤝 PAREGGIO! La griglia è piena.\nVuoi giocare ancora?", "Pareggio"));
                    }
                    else if (messaggio.equals("REMATCH_OK")) {
                        SwingUtilities.invokeLater(() -> {
                            for (int r = 0; r < 6; r++) {
                                for (int c = 0; c < 7; c++) {
                                    bottoni[r][c].setBackground(Color.WHITE);
                                }
                            }
                        });
                    }
                    // --- NUOVA GESTIONE DELL'ABBANDONO ---
                    else if (messaggio.equals("ABBANDONO")) {
                        SwingUtilities.invokeLater(() -> {
                            JOptionPane.showMessageDialog(frame, "L'avversario ha rifiutato la rivincita ed è fuggito.", "Partita Terminata", JOptionPane.WARNING_MESSAGE);
                            frame.dispose();
                            System.exit(0);
                        });
                    }
                }
            } catch (IOException e) {
                System.out.println("Connessione col server persa.");
            }
        }).start();
    }

    private void gestisciFinePartita(String testo, String titolo) {
        int risposta = JOptionPane.showConfirmDialog(frame, testo, titolo, JOptionPane.YES_NO_OPTION);
        if (risposta == JOptionPane.YES_OPTION) {
            out.println("REMATCH"); 
        } else {
            out.println("NO_REMATCH"); // Avvisa il server prima di morire
            frame.dispose(); 
            System.exit(0);
        }
    }
}