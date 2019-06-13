package com.bbn.godec.server;

import org.eclipse.jetty.util.security.Password;

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Scanner;

public class PasswordTool
{
	public static void main(String [ ] args) throws Exception
	{
		ArrayList<String> pwds = new ArrayList<String>();
		for(int argIdx = 1; argIdx < args.length; argIdx++)
		{
			pwds.add(args[argIdx]);
		}
		// NOTE TO CODE REVIEWER:  THIS FILE IS A UTILITY USED AT INSTALLATION TIME BY A SYSTEM ADMINISTRATOR AND NOT AT RUNTIME
		writeObfuscatedPasswordsFile(new File(args[0]), pwds);
	}


	/**
	 * Read passwords from obfuscated file
	 * @param pwdFile
	 * @return
	 * @throws Exception
	 */

	public static ArrayList<String> readObfuscatedPasswordsFile(File pwdFile) throws Exception
	{
		ArrayList<String> pwds = new ArrayList<String>();
		Scanner scanner =  new Scanner(pwdFile);
		while(scanner.hasNextLine())
		{
		  pwds.add(Password.deobfuscate("OBF:"+scanner.nextLine()));
		}
		return pwds;
	}

	/**
	 * Write passwords to obfuscated file
	 * @param pwdFile
	 * @param pwds
	 * @throws Exception
	 */
	public static void writeObfuscatedPasswordsFile(File pwdFile, ArrayList<String> pwds) throws Exception
	{
		PrintWriter writer = new PrintWriter(pwdFile, "UTF-8");
		for(String pwd : pwds)
		{
			writer.println(Password.obfuscate(pwd).substring("OBF:".length()));
		}
		writer.close();
	}

}
