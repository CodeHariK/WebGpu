import { BrowserRouter as Router, Routes, Route } from "react-router-dom";
import Dashboard from "./components/Dashboard";
import ImagesPage from "./pages/ImagesPage";
import VolumesPage from "./pages/VolumesPage";
import NetworksPage from "./pages/NetworksPage";
import ContainerDetailsPage from "./pages/ContainerDetailsPage";
import DockerfilesPage from "./pages/DockerfilesPage";
import RegistriesPage from "./pages/RegistriesPage";
import SettingsPage from "./pages/SettingsPage";

function App() {
  return (
    <Router>
      <Routes>
        <Route path="/" element={<Dashboard />} />
        <Route path="/images" element={<ImagesPage />} />
        <Route path="/volumes" element={<VolumesPage />} />
        <Route path="/networks" element={<NetworksPage />} />
        <Route path="/container/:id" element={<ContainerDetailsPage />} />
        <Route path="/dockerfiles" element={<DockerfilesPage />} />
        <Route path="/registries" element={<RegistriesPage />} />
        <Route path="/settings" element={<SettingsPage />} />
        {/* We'll add remaining pages later */}
        <Route path="*" element={<Dashboard />} />
      </Routes>
    </Router>
  );
}

export default App;
