use simpleargumentsparser::parse_cli;
use std::process;
use bahamut_core::BahamutModule;
use domain_fetcher::DomainFetcher;

fn main() {
  let cli = parse_cli();

  if cli.no_args || cli.s.contains_key("h") || cli.c.contains_key("help") {
    print_help(&cli);
    process::exit(0);
  }

  let target = match cli.c.get("target").or_else(|| cli.s.get("t")) {
    Some(t) => t,
    None => {
      println!("{} target required", cli.color.red("error:"));
      process::exit(1);
    }
  };

  if cli.c.contains_key("fetch") {
    let module = DomainFetcher;
    execute_module(&module, target, &cli);
  }
}

fn execute_module(module: &dyn BahamutModule, target: &str, cli: &simpleargumentsparser::CLI) {
  println!("{} Running: {}", cli.color.green("▶"), module.name());
  if let Err(e) = module.execute(target) {
    println!("{} {} failed: {}", cli.color.red("✗"), module.name(), e);
  }
}

fn print_help(cli: &simpleargumentsparser::CLI) {
  println!("{}", cli.color.bold().bright_red("BAHAMUT"));
  println!("\nUsage:\n  bahamut -t <target> [options]");
  println!("\nOptions:");
  println!("  -t, --target  Target URL");
  println!("  --fetch       Run domain fetcher");
}
