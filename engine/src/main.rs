pub trait BahamutModule {
    fn name(&self) -> &str;
    fn description(&self) -> &str;
    fn execute(&self, target: &str) -> anyhow::Result<()>;
}

fn main() {
    println!("Bahamut Engine loaded...");
}
