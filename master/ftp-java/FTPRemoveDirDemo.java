import java.io.IOException;

import org.apache.commons.net.ftp.FTPClient;
import org.apache.commons.net.ftp.FTPReply;

public class FTPRemoveDirDemo {

	public static void main(String[] args) {
		String server = "www.myserver.com";
		int port = 21;
		String user = "user";
		String pass = "pass";

		FTPClient ftpClient = new FTPClient();
		try {

			ftpClient.connect(server, port);

			int replyCode = ftpClient.getReplyCode();
			if (!FTPReply.isPositiveCompletion(replyCode)) {
				System.out.println("Connect failed");
				return;
			}

			boolean success = ftpClient.login(user, pass);

			if (!success) {
				System.out.println("Could not login to the server");
				return;
			}

			String dirToRemove = "/public/video/temp";

			boolean deleted = ftpClient.removeDirectory(dirToRemove);
			if (deleted) {
				System.out.println("The directory was removed successfully.");
			} else {
				System.out.println("Could not delete the directory, it may not be empty.");
			}

		} catch (IOException ex) {
			System.out.println("Oh no, there was an error: " + ex.getMessage());
			ex.printStackTrace();
		} finally {
			// logs out and disconnects from server
			try {
				if (ftpClient.isConnected()) {
					ftpClient.logout();
					ftpClient.disconnect();
				}
			} catch (IOException ex) {
				ex.printStackTrace();
			}
		}
	}

}
