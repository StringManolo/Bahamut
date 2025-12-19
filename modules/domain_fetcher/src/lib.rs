use bahamut_core::BahamutModule;

pub struct DomainFetcher;

impl BahamutModule for DomainFetcher {
  fn name(&self) -> &str {
    "Domain Fetcher"
  }

  fn description(&self) -> &str {
    "Downloads public bug hunting domains"
  }

  fn execute(&self, target: &str) -> anyhow::Result<()> {
    println!("Fetching domains for target: {}", target);
    Ok(())
  }
}
