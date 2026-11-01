//! Corporate Agency Mesh Architecture
//! Implements Managing Director + 6 Departmental Specialists per Master Spec §3.1 & §3.2.

pub mod director;
pub mod finance;
pub mod hr;
pub mod legal;
pub mod operations;
pub mod procurement;
pub mod trade;

pub use director::ManagingDirector;
pub use finance::FinanceSpecialist;
pub use hr::HrSpecialist;
pub use legal::LegalSpecialist;
pub use operations::OperationsSpecialist;
pub use procurement::ProcurementSpecialist;
pub use trade::TradeSpecialist;

use crate::safety::SafetyGovernor;
use serde::{Deserialize, Serialize};
use std::fmt;

/// Departments within the Corporate Agency Mesh
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Department {
    Executive,
    Finance,
    Legal,
    Trade,
    Hr,
    Procurement,
    Operations,
}

impl fmt::Display for Department {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Department::Executive => write!(f, "Executive"),
            Department::Finance => write!(f, "Finance & FinOps"),
            Department::Legal => write!(f, "Legal & Compliance"),
            Department::Trade => write!(f, "Trade & Customs"),
            Department::Hr => write!(f, "HR & People"),
            Department::Procurement => write!(f, "Procurement"),
            Department::Operations => write!(f, "Operations"),
        }
    }
}

impl std::str::FromStr for Department {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().trim() {
            "executive" | "director" | "md" | "cos" => Ok(Department::Executive),
            "finance" | "finops" => Ok(Department::Finance),
            "legal" | "compliance" => Ok(Department::Legal),
            "trade" | "customs" | "mirsal" => Ok(Department::Trade),
            "hr" | "people" | "labor" => Ok(Department::Hr),
            "procurement" | "vendor" => Ok(Department::Procurement),
            "operations" | "ops" | "facilities" => Ok(Department::Operations),
            _ => Err(format!("Unknown department: '{}'", s)),
        }
    }
}

/// Specialist Verdict
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(tag = "status", rename_all = "snake_case")]
pub enum SpecialistVerdict {
    Approved,
    Caution { reason: String },
    Gated {
        required_scope: String,
        amount_aed: f64,
        reason: String,
    },
    Rejected {
        code: String,
        reason: String,
    },
}

/// Mesh Task representation
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MeshTask {
    pub task_id: String,
    pub department: Department,
    pub action: String,
    pub payload: serde_json::Value,
}

impl MeshTask {
    pub fn new(
        task_id: impl Into<String>,
        department: Department,
        action: impl Into<String>,
        payload: serde_json::Value,
    ) -> Self {
        Self {
            task_id: task_id.into(),
            department,
            action: action.into(),
            payload,
        }
    }
}

/// Specialist Response representation
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SpecialistResponse {
    pub specialist_name: String,
    pub department: Department,
    pub action: String,
    pub verdict: SpecialistVerdict,
    pub data: serde_json::Value,
    pub statutory_citations: Vec<String>,
    pub execution_time_ms: f64,
}

/// Trait implemented by all Departmental Specialists
pub trait DepartmentSpecialist: Send + Sync {
    fn name(&self) -> &'static str;
    fn department(&self) -> Department;
    fn status(&self) -> &'static str;
    fn capabilities(&self) -> Vec<&'static str>;
    fn handle_task(
        &mut self,
        task: &MeshTask,
        governor: &mut SafetyGovernor,
    ) -> Result<SpecialistResponse, String>;
}

/// Information status descriptor for mesh health reporting
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SpecialistStatusInfo {
    pub name: String,
    pub department: Department,
    pub status: String,
    pub capabilities: Vec<String>,
}

/// Corporate Agency Mesh runtime holding all specialists and the Governor
pub struct AgencyMesh {
    pub director: ManagingDirector,
    pub finance: FinanceSpecialist,
    pub legal: LegalSpecialist,
    pub trade: TradeSpecialist,
    pub hr: HrSpecialist,
    pub procurement: ProcurementSpecialist,
    pub operations: OperationsSpecialist,
    pub governor: SafetyGovernor,
}

impl AgencyMesh {
    pub fn new(governor: SafetyGovernor) -> Self {
        Self {
            director: ManagingDirector::new(),
            finance: FinanceSpecialist::new(),
            legal: LegalSpecialist::new(),
            trade: TradeSpecialist::new(),
            hr: HrSpecialist::new(),
            procurement: ProcurementSpecialist::new(),
            operations: OperationsSpecialist::new(),
            governor,
        }
    }

    /// Dispatch a task to the appropriate specialist
    pub fn dispatch(&mut self, task: &MeshTask) -> Result<SpecialistResponse, String> {
        match task.department {
            Department::Executive => self.director.handle_executive_task(
                task,
                &mut self.finance,
                &mut self.legal,
                &mut self.trade,
                &mut self.hr,
                &mut self.procurement,
                &mut self.operations,
                &mut self.governor,
            ),
            Department::Finance => self.finance.handle_task(task, &mut self.governor),
            Department::Legal => self.legal.handle_task(task, &mut self.governor),
            Department::Trade => self.trade.handle_task(task, &mut self.governor),
            Department::Hr => self.hr.handle_task(task, &mut self.governor),
            Department::Procurement => self.procurement.handle_task(task, &mut self.governor),
            Department::Operations => self.operations.handle_task(task, &mut self.governor),
        }
    }

    /// Returns status report of all mesh agents
    pub fn status_report(&self) -> Vec<SpecialistStatusInfo> {
        vec![
            SpecialistStatusInfo {
                name: self.director.name().to_string(),
                department: Department::Executive,
                status: self.director.status().to_string(),
                capabilities: self.director.capabilities().into_iter().map(String::from).collect(),
            },
            SpecialistStatusInfo {
                name: self.finance.name().to_string(),
                department: Department::Finance,
                status: self.finance.status().to_string(),
                capabilities: self.finance.capabilities().into_iter().map(String::from).collect(),
            },
            SpecialistStatusInfo {
                name: self.legal.name().to_string(),
                department: Department::Legal,
                status: self.legal.status().to_string(),
                capabilities: self.legal.capabilities().into_iter().map(String::from).collect(),
            },
            SpecialistStatusInfo {
                name: self.trade.name().to_string(),
                department: Department::Trade,
                status: self.trade.status().to_string(),
                capabilities: self.trade.capabilities().into_iter().map(String::from).collect(),
            },
            SpecialistStatusInfo {
                name: self.hr.name().to_string(),
                department: Department::Hr,
                status: self.hr.status().to_string(),
                capabilities: self.hr.capabilities().into_iter().map(String::from).collect(),
            },
            SpecialistStatusInfo {
                name: self.procurement.name().to_string(),
                department: Department::Procurement,
                status: self.procurement.status().to_string(),
                capabilities: self.procurement.capabilities().into_iter().map(String::from).collect(),
            },
            SpecialistStatusInfo {
                name: self.operations.name().to_string(),
                department: Department::Operations,
                status: self.operations.status().to_string(),
                capabilities: self.operations.capabilities().into_iter().map(String::from).collect(),
            },
        ]
    }
}
