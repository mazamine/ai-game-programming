import java.io.*;
import java.util.concurrent.*;

public class Arbitre {

    // === PARAMÈTRES ===
    private static final int TIMEOUT_SECONDS = 20;
    private static final int MAX_TURNS = 400;

    public static void main(String[] args) throws Exception {

        if (args.length < 2) {
            System.err.println("Usage: java Arbitre <botA> <botB>");
            return;
        }

        String botAPath = args[0];
        String botBPath = args[1];

        System.out.println("Match: " + botAPath + " vs " + botBPath);

        // === LANCEMENT DES PROCESSUS ===
        Process botA = new ProcessBuilder(botAPath).redirectError(ProcessBuilder.Redirect.INHERIT).start();
        Process botB = new ProcessBuilder(botBPath).redirectError(ProcessBuilder.Redirect.INHERIT).start();

        Player A = new Player("A", botA);
        Player B = new Player("B", botB);

        Player current = A;
        Player other = B;

        String lastMove = "START";
        int turn = 0;

        System.out.println("=== DÉBUT DE LA PARTIE ===");

        while (turn < MAX_TURNS) {

            // 1) envoyer le dernier coup
            try {
                current.send(lastMove);
            } catch (IOException e) {
                disqualify(current, "Impossible d'écrire (process mort)");
                break;
            }

            // 2) attendre la réponse
            String response = current.receive(TIMEOUT_SECONDS);

            if (response == null) {
                disqualify(current, "TIMEOUT (" + TIMEOUT_SECONDS + "s)");
                break;
            }

            response = response.trim();

            turn++;
            System.out.println("[Tour " + turn + "] Joueur " + current.name + " joue : " + response);

            // 3) préparer le tour suivant
            lastMove = response;

            Player tmp = current;
            current = other;
            other = tmp;
        }

        if (turn >= MAX_TURNS) {
            System.out.println("RESULT : FIN PAR LIMITE DE COUPS (" + MAX_TURNS + ")");
        }

        // === NETTOYAGE ===
        try {
            A.send("END");
            B.send("END");
        } catch (Exception ignored) {}

        A.destroy();
        B.destroy();

        System.out.println("=== FIN DE LA PARTIE ===");
    }

    private static void disqualify(Player p, String reason) {
        System.out.println("RESULT : Joueur " + p.name + " DISQUALIFIÉ (" + reason + ")");
    }

    // =========================================================
    // Classe Player : encapsule stdin/stdout + timeout
    // =========================================================
    static class Player {

        String name;
        Process process;
        BufferedWriter out;
        BufferedReader in;
        ExecutorService executor = Executors.newSingleThreadExecutor();

        Player(String name, Process p) {
            this.name = name;
            this.process = p;
            this.out = new BufferedWriter(new OutputStreamWriter(p.getOutputStream()));
            this.in = new BufferedReader(new InputStreamReader(p.getInputStream()));
        }

        void send(String msg) throws IOException {
            out.write(msg);
            out.newLine();
            out.flush();
        }

        String receive(int timeoutSeconds) {
            Future<String> future = executor.submit(() -> in.readLine());
            try {
                return future.get(timeoutSeconds, TimeUnit.SECONDS);
            } catch (TimeoutException e) {
                future.cancel(true);
                return null;
            } catch (Exception e) {
                return null;
            }
        }

        void destroy() {
            executor.shutdownNow();
            if (process.isAlive()) {
                process.destroyForcibly();
            }
        }
    }
}
